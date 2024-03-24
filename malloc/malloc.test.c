#include <stdio.h>
#include <string.h>

#include "testlib.h"
#include "malloc.h"

static void
successful_malloc_returns_non_null_pointer(void)
{
	char *var = my_malloc(100);

	ASSERT_TRUE("successful malloc returns non null pointer", var != NULL);

	my_free(var);
}

static void
correct_copied_value(void)
{
	char test_string[100] = "FISOP malloc is working!";

	char *var = my_malloc(100);

	strcpy(var, test_string);


	ASSERT_TRUE("allocated memory should contain the copied value",
	            strcmp(var, test_string) == 0);

	my_free(var);
}

static void
correct_amount_of_mallocs(void)
{
	struct malloc_stats stats;

	char *var = my_malloc(100);

	my_free(var);

	get_stats(&stats);
	ASSERT_TRUE("amount of mallocs should be one", stats.mallocs == 1);
}

static void
correct_amount_of_frees(void)
{
	struct malloc_stats stats;

	char *var = my_malloc(100);

	my_free(var);

	get_stats(&stats);

	ASSERT_TRUE("amount of frees should be one", stats.frees == 1);
}

static void
correct_amount_of_requested_memory(void)
{
	struct malloc_stats stats;

	char *var = my_malloc(100);

	my_free(var);

	get_stats(&stats);


	ASSERT_TRUE("amount of requested memory should be 100",
	            stats.requested_memory == 100);
}

static void
malloc_fails_if_request_is_too_big(void)
{
	char *var = my_malloc(1000000000);

	ASSERT_TRUE("malloc returns null when asked for too much memory", !var);
}

static void
realloc_on_null_pointer_returns_non_null_pointer()
{
	struct malloc_stats stats;
	char *var = my_realloc(NULL, 100);
	ASSERT_TRUE("successful realloc returns non null pointer", var != NULL);
	my_free(var);
}

static void
stats_are_correct_with_realloc()
{
	struct malloc_stats stats;
	char *var = my_realloc(NULL, 100);
	get_stats(&stats);
	ASSERT_TRUE("amount of requested memory should be 100",
	            stats.requested_memory == 100);
	ASSERT_TRUE("amount of mallocs should be one", stats.mallocs == 1);

	char *aux = my_realloc(var, 150);
	get_stats(&stats);
	ASSERT_TRUE("successful realloc returns non null pointer", aux != NULL);
	ASSERT_TRUE("amount of mallocs should be one", stats.mallocs == 1);
	var = aux;
	my_free(var);
	get_stats(&stats);
	ASSERT_TRUE("amount of frees should be one", stats.frees == 1);
}

static void
realloc_fails_if_request_is_too_big()
{
	struct malloc_stats stats;
	char *var = my_realloc(NULL, 100);
	char *aux = my_realloc(var, 1000000000);
	ASSERT_TRUE("realloc returns null when asked for too much memory", !aux);
	my_free(var);
}

static void
realloc_contains_the_copied_value()
{
	char first_test_string[100] =
	        "FISOP realloc is working with NULL pointer!";
	char *var = my_realloc(NULL, 100);
	strcpy(var, first_test_string);
	ASSERT_TRUE("allocated memory should contain the copied value",
	            strcmp(var, first_test_string) == 0);

	char second_test_string[200] = "FISOP realloc is working!";
	char *aux = my_realloc(var, 200);
	if (aux)
		var = aux;
	strcpy(var, second_test_string);
	ASSERT_TRUE("reallocated memory should contain the copied value",
	            strcmp(var, second_test_string) == 0);
	my_free(var);
}

static void
to_realloc_to_size_zero_is_to_free()
{
	struct malloc_stats stats;
	char *var = my_malloc(300);
	ASSERT_TRUE("successful malloc returns non null pointer", var != NULL);
	char *aux = my_realloc(var, 0);
	get_stats(&stats);
	ASSERT_TRUE("realloc returns null when asked to realloc to size 0", !aux);
	ASSERT_TRUE("amount of frees should be at least one (maybe had to free "
	            "the old block and copy into a completely new one)",
	            stats.frees >= 1);
}

struct grupo_fisop {
	char *manu;
	char *noah;
	char *cami;
	int nota;
};

static void
calloc_returns_null_initialized_memory()
{
	struct grupo_fisop *grupo = my_calloc(1, 100);

	ASSERT_TRUE(
	        "allocated memory is inicialized with NULL when calling calloc",
	        grupo->manu == NULL && grupo->noah == NULL &&
	                grupo->cami == NULL && grupo->nota == 0);

	my_free(grupo);
}

static void
splitting_and_coleascing_work_seamlessly_for_small_blocks()
{
	struct malloc_stats stats;
	char *x = my_malloc(500);
	if (!x)
		return;
	char *realloc = my_realloc(x, 914);
	get_stats(&stats);
	ASSERT_TRUE("when realloc is made in a small block with available "
	            "space, regions coleasce",
	            stats.mallocs == 1 && stats.frees == 0);

	char *realloc2 = my_realloc(x, 400);
	get_stats(&stats);
	ASSERT_TRUE(
	        "when realloc asks for less memory than it had, regions split",
	        stats.mallocs == 1 && stats.frees == 0);

	my_free(realloc2);
}
static void
splitting_and_coleascing_work_seamlessly_for_medium_blocks()
{
	struct malloc_stats stats;
	char *x = my_malloc(50000);
	if (!x)
		return;
	char *realloc = my_realloc(x, 91400);
	get_stats(&stats);
	ASSERT_TRUE("when realloc is made in a medium block with available "
	            "space, regions coleasce",
	            stats.mallocs == 1 && stats.frees == 0);

	char *realloc2 = my_realloc(x, 17000);
	get_stats(&stats);
	ASSERT_TRUE("when realloc asks for less memory regions split",
	            stats.mallocs == 1 && stats.frees == 0);

	char *realloc3 = my_realloc(x, 17);
	get_stats(&stats);
	ASSERT_TRUE("when realloc asks for much less memory regions split",
	            stats.mallocs == 1 && stats.frees == 0);

	my_free(realloc3);
}

static void
splitting_and_coleascing_work_seamlessly_for_big_blocks()
{
	struct malloc_stats stats;
	char *x = my_malloc(1100000);
	if (!x)
		return;
	char *realloc = my_realloc(x, 1500000);
	get_stats(&stats);
	ASSERT_TRUE("when realloc is made in an almost empty block regions "
	            "coleasce",
	            stats.mallocs == 1 && stats.frees == 0);

	char *realloc2 = my_realloc(x, 1050000);
	get_stats(&stats);
	ASSERT_TRUE("when realloc asks for less memory regions split",
	            stats.mallocs == 1 && stats.frees == 0);

	char *realloc3 = my_realloc(x, 17);
	get_stats(&stats);
	ASSERT_TRUE("when realloc asks for much less memory regions split",
	            stats.mallocs == 1 && stats.frees == 0);

	my_free(realloc3);
}

static void
correct_copied_value_with_realloc(void)
{
	char test_string[100] = "FISOP malloc is working!";

	char *var = my_malloc(100);

	strcpy(var, test_string);

	char *var_new = my_realloc(var, 200);
	printf("var new: %s\n", var_new);
	ASSERT_TRUE("reallocked memory should contain the copied value",
	            strcmp(var_new, test_string) == 0);

	my_free(var_new);
}

int
main(void)
{
	run_test(successful_malloc_returns_non_null_pointer);
	run_test(malloc_fails_if_request_is_too_big);
	run_test(correct_copied_value);
	run_test(correct_amount_of_mallocs);
	run_test(correct_amount_of_frees);
	run_test(correct_amount_of_requested_memory);
	run_test(realloc_on_null_pointer_returns_non_null_pointer);
	run_test(stats_are_correct_with_realloc);
	run_test(realloc_fails_if_request_is_too_big);
	run_test(realloc_contains_the_copied_value);
	run_test(to_realloc_to_size_zero_is_to_free);
	run_test(calloc_returns_null_initialized_memory);
	run_test(splitting_and_coleascing_work_seamlessly_for_small_blocks);
	run_test(splitting_and_coleascing_work_seamlessly_for_medium_blocks);
	run_test(splitting_and_coleascing_work_seamlessly_for_big_blocks);
	run_test(correct_copied_value_with_realloc);
	return 0;
}
