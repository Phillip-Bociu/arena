#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define ARENA_IMPLEMENTATION
#define THIS_IS_FOR_SURE_A_SAFE_MACRO_NAME_TO_USE_FOR_TESTING_RIGHT

#include "doctest.h"
#include "../Arena.hpp"
#include <thread>

TEST_CASE("Arena") {
	arn::Arena& arena = *arn::allocArena(1024);
	const size_t allocSize = 128;
	void* idk = arena.alloc(allocSize);

	CHECK(arena.size == allocSize);
	CHECK(idk != nullptr);
	CHECK((size_t)idk % 8 == 0);

	SUBCASE("Not enough space") {
		void* test = malloc(sizeof(arn::Arena) - 1);
		arn::Arena* t2 = arn::createArena(test, sizeof(arn::Arena) - 1);
		CHECK(t2 == nullptr);
		free(test);
	}

	SUBCASE("too large") {
		const size_t currentSize = arena.size;
		void* test = arena.alloc(10000000);
		CHECK(test == nullptr);
		CHECK(arena.size == currentSize);
	}

	SUBCASE("clear") {
		arena.clear();
		CHECK(arena.size == 0);
	}

	SUBCASE("alignment") {
		for(int i = 0; i != 8; i++) {
			char* t = (char*)arena.alloc(1, 1);
			CHECK(((size_t)t % 8) == (i % 8));
		}
	}

	SUBCASE("clearOnScopeExit") {
		{
			arn::ScopeExit se = arena.clearOnScopeExit();
		}
		CHECK(arena.size == 0);
	}

	SUBCASE("restoreToCurrentSizeOnScopeExit") {
		const size_t currentSize = arena.size;
		{
			arn::ScopeExit se = arena.restoreToCurrentSizeOnScopeExit();
			void* testAlloc = arena.alloc(100);
		}
		CHECK(arena.size == currentSize);
	}
	arn::freeArena(arena);
}

