#pragma once
#include <cstdint>
#include <mutex>

/*==================================
				Macros
  ==================================*/

#ifndef ARENA_ALLOC
#include <cstdlib>
#define ARENA_ALLOC(size, alignment) malloc(size)
#endif

#ifndef ARENA_FREE
#include <cstdlib>
#define ARENA_FREE(ptr, size) free(ptr)
#endif

#if __cplusplus >= 201703L
#define NODISCARD [[nodiscard]]
#else 
#define NODISCARD
#endif

#ifdef THIS_IS_FOR_SURE_A_SAFE_MACRO_NAME_TO_USE_FOR_TESTING_RIGHT
#define PRIVATE public
#else
#define PRIVATE private
#endif

namespace arn {
	/*==================================
		Type definitions

				I hate them too, but I want this
				to be approved at work
	  ==================================*/
	struct NODISCARD ScopeExit {
			~ScopeExit();

			friend struct Arena;
		PRIVATE:
			ScopeExit(Arena* arena, const size_t sizeToSetTheArenaTo = 0):arena(arena), sizeToSetTheArenaTo(sizeToSetTheArenaTo) {}
			struct Arena* arena;
			uint64_t sizeToSetTheArenaTo;
	};

	struct NODISCARD Arena {
	NODISCARD void* alloc(size_t size, size_t aligmnent = alignof(std::max_align_t));
	void clear();
	ScopeExit clearOnScopeExit();
	ScopeExit restoreToCurrentSizeOnScopeExit();
PRIVATE:
		size_t size;
		size_t capacity;
		uint8_t bytes[];
	};

	struct NODISCARD Ref {
			Ref() {}
			~Ref();
			Ref(Ref&& other);
			Ref& operator=(Ref&& other);

			Ref(const Ref& other) = delete;
			Ref& operator=(const Ref& other) = delete;
			friend struct MtArena;
		PRIVATE:
			struct MtArena* arena;
	};

	struct MtArena {
		NODISCARD void* alloc(size_t size, size_t aligmnent = alignof(std::max_align_t));
		NODISCARD Ref ref();
PRIVATE:
		std::mutex mut;
		size_t refCount; // used for clearing
		size_t size;
		size_t capacity;
		uint8_t bytes[];

		friend struct Ref;
	};

	/*==================================
			 Function declarations
	  ==================================*/

	// creates non-synchronized arena from provided memory
	// 'memory' will be owned by the arena during its lifetime
	NODISCARD Arena* createArena(void* memory, size_t memorySize);

	// creates internally synchronized arena from provided memory
	// 'memory' will be owned by the arena during its lifetime
	NODISCARD MtArena* createMtArena(void* memory, size_t memorySize);

	// allocates (with the replaceable ARENA_ALLOC macro) and initializes arena.
	NODISCARD Arena* allocArena(size_t capacity);

	// allocates (with the replaceable ARENA_ALLOC macro) and initializes multi-threaded arena.
	NODISCARD MtArena* allocMtArena(size_t capacity);

	// frees arena allocated with allocArena
	void freeArena(Arena* arena);

	// frees multi-threaded arena allocated with allocArena
	void freeArena(MtArena* arena);

	/*==================================
		Implementation

				This is split into sections:
					- Initialization Functions
					- Allocation Functions 
					- Clearing 
					- References 
	  ==================================*/
#ifdef ARENA_IMPLEMENTATION

	// ----------- Initialization Functions -----------
	Arena* createArena(void* memory, size_t memorySize) {
		if(memorySize <= sizeof(Arena)) return nullptr;

		Arena* retval = static_cast<Arena*>(memory);
		retval->size = 0;
		retval->capacity = memorySize - sizeof(Arena);
		return retval;
	}

	MtArena* createMtArena(void* memory, size_t memorySize) {
		if(memorySize <= sizeof(MtArena)) return nullptr;

		MtArena* retval = static_cast<MtArena*>(memory);
		retval->size = 0;
		retval->capacity = memorySize - sizeof(MtArena);
		new(&retval->mut) std::mutex();
		return retval;
	}

	Arena* allocArena(size_t capacity) {
		void* memory = ARENA_ALLOC(sizeof(Arena) + capacity, alignof(Arena));
		return createArena(memory, sizeof(Arena) + capacity);
	}

	MtArena* allocMtArena(size_t capacity) {
		void* memory = ARENA_ALLOC(sizeof(MtArena) + capacity, alignof(MtArena));
		return createMtArena(memory, sizeof(MtArena) + capacity);
	}

	void freeArena(Arena& arena) {
		ARENA_FREE(&arena, sizeof(Arena) + arena.capacity);
	}

	void freeArena(MtArena& arena) {
		ARENA_FREE(&arena, sizeof(MtArena) + arena.capacity);
	}

	// ----------- Allocation Functions -----------

	void* Arena::alloc(const size_t allocSize, const size_t alignment) {
        const size_t alignmentDistance = (alignment - (this->size % alignment)) % alignment;
		if(this->size + allocSize + alignment > this->capacity) {
			return nullptr;
		}

		void* retval = &this->bytes[this->size + alignmentDistance];
		this->size += alignmentDistance + allocSize;

		return retval;
	}

	void* MtArena::alloc(const size_t size, const size_t alignment) {
		std::unique_lock<std::mutex> lck(this->mut);

        const size_t alignmentDistance = (alignment - (this->size % alignment)) % alignment;
		if(this->size + size + alignment > this->capacity) {
			return nullptr;
		}

		void* retval = &this->bytes[this->size + alignmentDistance];
		this->size += alignmentDistance + size;

		return retval;
	}

	// ----------- Clearing -----------
	void Arena::clear() {
		size = 0;
	}

	ScopeExit Arena::clearOnScopeExit() {
		return ScopeExit(this, 0);
	}

	ScopeExit Arena::restoreToCurrentSizeOnScopeExit() {
		return ScopeExit(this, this->size);
	}

	ScopeExit::~ScopeExit() {
		arena->size = sizeToSetTheArenaTo;
	}

	// ----------- References -----------
	Ref::~Ref() {
		if(!arena) return;

		arena->mut.lock();

		arena->refCount--;
		if(arena->refCount == 0) {
			arena->size = 0;
		}

		arena->mut.unlock();
	}

	Ref::Ref(Ref&& other) {
		arena = other.arena;
		other.arena = nullptr;
	}

	Ref& Ref::operator=(Ref&& other) {
		arena = other.arena;
		other.arena = nullptr;
		return *this;
	}

	Ref ref(MtArena& arena) {
		arena.mut.lock();

		arena.refCount++;
		Ref retval;
		retval.arena = &arena;

		arena.mut.unlock();
		return retval;
	}

#endif
}
#undef NODISCARD 
