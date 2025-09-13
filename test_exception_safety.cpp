#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdint>

// Declare the C interface function
extern "C" {
char *xmutil_convert_mdl_to_xmile(const char *mdlSource, uint32_t mdlSourceLen, const char *fileName, bool isCompact,
                                  int isLongName, bool isAsSectors);
}

int main() {
    printf("Testing exception safety with malformed input...\n");

    // Test 1: Invalid MDL file with syntax errors
    const char* invalid_mdl = "This is not valid MDL syntax @#$%^&*()";
    char* result = xmutil_convert_mdl_to_xmile(invalid_mdl, strlen(invalid_mdl), "test.mdl", false, 0, false);
    if (result == nullptr) {
        printf("Test 1 PASSED: Invalid MDL returned nullptr without crashing\n");
    } else {
        printf("Test 1 FAILED: Expected nullptr for invalid MDL\n");
        free(result);
    }

    // Test 2: Empty input
    const char* empty_mdl = "";
    result = xmutil_convert_mdl_to_xmile(empty_mdl, 0, "test.mdl", false, 0, false);
    if (result == nullptr) {
        printf("Test 2 PASSED: Empty MDL returned nullptr without crashing\n");
    } else {
        printf("Test 2 FAILED: Expected nullptr for empty MDL\n");
        free(result);
    }

    // Test 3: MDL with incomplete equation that would trigger parser error
    const char* incomplete_mdl = "Variable = ";
    result = xmutil_convert_mdl_to_xmile(incomplete_mdl, strlen(incomplete_mdl), "test.mdl", false, 0, false);
    if (result == nullptr) {
        printf("Test 3 PASSED: Incomplete equation returned nullptr without crashing\n");
    } else {
        printf("Test 3 FAILED: Expected nullptr for incomplete equation\n");
        free(result);
    }

    // Test 4: MDL with type mismatch error
    const char* type_mismatch_mdl = "VAR[SUB1] = 10\nVAR = \"not a subscript\"\n";
    result = xmutil_convert_mdl_to_xmile(type_mismatch_mdl, strlen(type_mismatch_mdl), "test.mdl", false, 0, false);
    if (result == nullptr) {
        printf("Test 4 PASSED: Type mismatch returned nullptr without crashing\n");
    } else {
        printf("Test 4 FAILED: Expected nullptr for type mismatch\n");
        free(result);
    }

    // Test 5: DYN file with invalid syntax
    const char* invalid_dyn = "L LEVEL.K=LEVEL.J+DT*(INFLOW.JK";
    result = xmutil_convert_mdl_to_xmile(invalid_dyn, strlen(invalid_dyn), "test.dyn", false, 0, false);
    if (result == nullptr) {
        printf("Test 5 PASSED: Invalid DYN returned nullptr without crashing\n");
    } else {
        printf("Test 5 FAILED: Expected nullptr for invalid DYN\n");
        free(result);
    }

    // Test 6: MDL with bad subscript range
    const char* bad_subscript = "VAR[SUB1:SUB1] = 10\n";
    result = xmutil_convert_mdl_to_xmile(bad_subscript, strlen(bad_subscript), "test.mdl", false, 0, false);
    if (result == nullptr) {
        printf("Test 6 PASSED: Bad subscript range returned nullptr without crashing\n");
    } else {
        printf("Test 6 FAILED: Expected nullptr for bad subscript range\n");
        free(result);
    }

    // Test 7: MDL with invalid number table
    const char* bad_table = "VAR = GET VDB CONSTANTS('data.vdb', 'invalid', 'not_a_number')\n";
    result = xmutil_convert_mdl_to_xmile(bad_table, strlen(bad_table), "test.mdl", false, 0, false);
    if (result == nullptr) {
        printf("Test 7 PASSED: Invalid number table returned nullptr without crashing\n");
    } else {
        printf("Test 7 FAILED: Expected nullptr for invalid number table\n");
        free(result);
    }

    // Test 8: Valid simple MDL (should succeed)
    const char* valid_mdl = "Stock = INTEG(Flow, 100)\n    ~ units\n    ~ comment |\nFlow = 10\n    ~ units/time\n    ~ |\n";
    result = xmutil_convert_mdl_to_xmile(valid_mdl, strlen(valid_mdl), "test.mdl", false, 0, false);
    if (result != nullptr) {
        printf("Test 8 PASSED: Valid MDL converted successfully\n");
        free(result);
    } else {
        printf("Test 8 FAILED: Valid MDL should have succeeded\n");
    }

    printf("\nAll tests completed. If no crashes occurred, exception safety is working!\n");
    return 0;
}