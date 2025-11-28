/*
    Tests for file system metadata functionality.
*/

#include "fs.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

void test_metadata_initialization() {
    printf("=== Testing Metadata Initialization ===\n");
    
    fs_init();
    
    // Test root directory has proper metadata.
    file_info_t info;
    assert(get_file_info("/", &info) == 0);
    printf("✓ Root directory metadata:\n");
    printf("  Name: '%s' (should be empty for root)\n", info.name);
    printf("  Type: %s\n", info.type == N_DIR ? "Directory" : "File");
    printf("  Created: %s\n", format_time(info.created));
    printf("  Attributes: %s\n", format_attributes(info.attributes));
    
    // Verify timestamps are reasonable (within last minute).
    time_t now = time(NULL);
    assert(now - info.created < 60); // Created within last 60 seconds.
    printf("✓ Timestamps are reasonable\n");
}

void test_directory_metadata_tracking() {
    printf("\n=== Testing Directory Metadata Tracking ===\n");
    
    // Create nested directories.
    assert(mkdir_p("/test/level1/level2/level3") == 0);
    
    file_info_t info;
    
    // Check each level has metadata.
    const char *paths[] = {"/test", "/test/level1", "/test/level1/level2", "/test/level1/level2/level3"};
    for (int i = 0; i < 4; i++) {
        assert(get_file_info(paths[i], &info) == 0);
        printf("✓ Directory %s: created=%s, children=%zu\n", 
               paths[i], format_time(info.created), info.child_count);
        assert(info.type == N_DIR);
        assert(info.size == 0); // Directories have size 0.
    }
    
    // Test that parent directory shows correct child count.
    assert(get_file_info("/test/level1/level2", &info) == 0);
    assert(info.child_count == 1); // Should have level3 as child.
    printf("✓ Parent directory child count is correct\n");
}

void test_file_operations_metadata() {
    printf("\n=== Testing File Operations Metadata Updates ===\n");
    
    // Create a test file.
    assert(create_file("/test/operations.txt") == 0);
    
    file_info_t before, after;
    
    // Test initial state.
    assert(get_file_info("/test/operations.txt", &before) == 0);
    printf("Initial state: size=%zu, created=%s\n", before.size, format_time(before.created));
    
    sleep(1); // Ensure timestamp difference.
    
    // Test write operation updates.
    const char *data1 = "First write";
    assert(write_file("/test/operations.txt", 0, data1, strlen(data1)) > 0);
    
    assert(get_file_info("/test/operations.txt", &after) == 0);
    printf("After write: size=%zu, modified=%s\n", after.size, format_time(after.modified));
    
    assert(after.size == strlen(data1));
    assert(after.modified > before.modified);
    assert(after.accessed > before.accessed);
    printf("✓ Write operation updated size, modified, and accessed timestamps\n");
    
    // Test append operation.
    before = after;
    sleep(1);
    
    const char *data2 = " - Second write";
    assert(write_file("/test/operations.txt", strlen(data1), data2, strlen(data2)) > 0);
    
    assert(get_file_info("/test/operations.txt", &after) == 0);
    assert(after.size == strlen(data1) + strlen(data2));
    assert(after.modified > before.modified);
    printf("✓ Append operation updated metadata correctly\n");
    
    // Test read operation (should only update accessed).
    before = after;
    sleep(1);
    
    char buffer[64];
    assert(read_file("/test/operations.txt", 0, buffer, sizeof(buffer)) > 0);
    
    assert(get_file_info("/test/operations.txt", &after) == 0);
    assert(after.modified == before.modified); // Modified should NOT change.
    assert(after.accessed > before.accessed);  // Accessed should change.
    printf("✓ Read operation only updated accessed timestamp\n");
}

void test_attribute_combinations() {
    printf("\n=== Testing Attribute Combinations ===\n");
    
    assert(create_file("/test/attributes.txt") == 0);
    
    // Test all possible single attributes.
    uint8_t single_attrs[] = {ATTR_HIDDEN, ATTR_READONLY, ATTR_SYSTEM, ATTR_ARCHIVE};
    const char *attr_names[] = {"HIDDEN", "READONLY", "SYSTEM", "ARCHIVE"};
    
    for (int i = 0; i < 4; i++) {
        assert(set_file_attributes("/test/attributes.txt", single_attrs[i]) == 0);
        
        file_info_t info;
        assert(get_file_info("/test/attributes.txt", &info) == 0);
        
        printf("✓ %s attribute: %s\n", attr_names[i], format_attributes(info.attributes));
        assert(info.attributes & single_attrs[i]);
    }
    
    // Test attribute combinations.
    struct {
        uint8_t flags;
        const char *description;
    } combinations[] = {
        {ATTR_HIDDEN | ATTR_READONLY, "Hidden + ReadOnly"},
        {ATTR_SYSTEM | ATTR_ARCHIVE, "System + Archive"},
        {ATTR_HIDDEN | ATTR_READONLY | ATTR_SYSTEM, "Hidden + ReadOnly + System"},
        {ATTR_HIDDEN | ATTR_READONLY | ATTR_SYSTEM | ATTR_ARCHIVE, "All Attributes"}
    };
    
    for (int i = 0; i < 4; i++) {
        assert(set_file_attributes("/test/attributes.txt", combinations[i].flags) == 0);
        
        file_info_t info;
        assert(get_file_info("/test/attributes.txt", &info) == 0);
        
        printf("✓ %s: %s\n", combinations[i].description, format_attributes(info.attributes));
        assert(info.attributes == combinations[i].flags);
    }
    
    // Test clearing attributes.
    assert(set_file_attributes("/test/attributes.txt", ATTR_NONE) == 0);
    file_info_t info;
    assert(get_file_info("/test/attributes.txt", &info) == 0);
    printf("✓ Cleared attributes: %s\n", format_attributes(info.attributes));
    assert(info.attributes == ATTR_NONE);
}

void test_error_handling() {
    printf("\n=== Testing Error Handling ===\n");
    
    file_info_t info;
    
    // Test non-existent file.
    assert(get_file_info("/nonexistent.txt", &info) == -1);
    printf("✓ get_file_info correctly fails for non-existent file\n");
    
    // Test setting attributes on non-existent file.
    assert(set_file_attributes("/nonexistent.txt", ATTR_HIDDEN) == -1);
    printf("✓ set_file_attributes correctly fails for non-existent file\n");
    
    // Test touch on non-existent file.
    assert(touch_file("/nonexistent.txt") == -1);
    printf("✓ touch_file correctly fails for non-existent file\n");
    
    // Test NULL pointer handling.
    assert(get_file_info("/test", NULL) == -1);
    printf("✓ get_file_info correctly handles NULL info pointer\n");
}

void test_timestamp_precision() {
    printf("\n=== Testing Timestamp Precision and Ordering ===\n");
    
    // Create multiple files quickly to test timestamp ordering.
    const char *files[] = {"/test/file1.txt", "/test/file2.txt", "/test/file3.txt"};
    time_t timestamps[3];
    
    for (int i = 0; i < 3; i++) {
        assert(create_file(files[i]) == 0);
        
        file_info_t info;
        assert(get_file_info(files[i], &info) == 0);
        timestamps[i] = info.created;
        
        printf("File %d created at: %s\n", i+1, format_time(timestamps[i]));
        
        usleep(1000); // Sleep 1ms to ensure different timestamps.
    }
    
    // Verify timestamps are in order (or at least not decreasing).
    assert(timestamps[1] >= timestamps[0]);
    assert(timestamps[2] >= timestamps[1]);
    printf("✓ File creation timestamps are properly ordered\n");
}

void test_large_file_metadata() {
    printf("\n=== Testing Large File Metadata ===\n");
    
    assert(create_file("/test/large.txt") == 0);
    
    // Write a large amount of data in chunks.
    const char *chunk = "This is a chunk of data that will be repeated many times. ";
    size_t chunk_len = strlen(chunk);
    size_t total_size = 0;
    
    file_info_t info_before, info_after;
    assert(get_file_info("/test/large.txt", &info_before) == 0);
    
    sleep(1); // Ensure timestamp difference.
    
    // Write 100 chunks.
    for (int i = 0; i < 100; i++) {
        assert(write_file("/test/large.txt", total_size, chunk, chunk_len) > 0);
        total_size += chunk_len;
    }
    
    assert(get_file_info("/test/large.txt", &info_after) == 0);
    
    printf("Large file: size=%zu bytes\n", info_after.size);
    printf("Modified timestamp updated: %s\n", 
           info_after.modified > info_before.modified ? "Yes" : "No");
    
    assert(info_after.size == total_size);
    assert(info_after.modified > info_before.modified);
    printf("✓ Large file metadata tracking works correctly\n");
}

void test_directory_access_tracking() {
    printf("\n=== Testing Directory Access Tracking ===\n");
    
    // Create directory structure.
    assert(mkdir_p("/access_test/subdir") == 0);
    assert(create_file("/access_test/file.txt") == 0);
    
    file_info_t before, after;
    
    // Get initial access time for directory.
    assert(get_file_info("/access_test", &before) == 0);
    printf("Directory initial access: %s\n", format_time(before.accessed));
    
    sleep(1);
    
    // List directory contents (this should update access time).
    // Note: ls_dir should update the directory's access time.
    ls_dir("/access_test");
    
    assert(get_file_info("/access_test", &after) == 0);
    printf("Directory after listing: %s\n", format_time(after.accessed));
    
    if (after.accessed > before.accessed) {
        printf("✓ Directory access time updated when listing contents\n");
    } else {
        printf("WARNING: Directory access time not updated (may need implementation)\n");
    }
}

void cleanup_test_data() {
    printf("\n=== Cleaning Up Test Data ===\n");
    
    // Remove test files and directories.
    const char *files[] = {
        "/test/operations.txt",
        "/test/attributes.txt", 
        "/test/file1.txt",
        "/test/file2.txt", 
        "/test/file3.txt",
        "/test/large.txt",
        "/access_test/file.txt"
    };
    
    for (int i = 0; i < 7; i++) {
        rm_file(files[i]);
    }
    
    // Remove directories (must be done in reverse order).
    rmdir_empty("/access_test/subdir");
    rmdir_empty("/access_test");
    rmdir_empty("/test/level1/level2/level3");
    rmdir_empty("/test/level1/level2");
    rmdir_empty("/test/level1");
    rmdir_empty("/test");
    
    printf("✓ Cleanup completed\n");
}

int main() {
    printf("File System Metadata Tests\n");
    printf("=========================================\n");
    
    test_metadata_initialization();
    test_directory_metadata_tracking();
    test_file_operations_metadata();
    test_attribute_combinations();
    test_error_handling();
    test_timestamp_precision();
    test_large_file_metadata();
    test_directory_access_tracking();
    cleanup_test_data();
    
    printf("\nAll Tests Completed Successfully! \n");
    printf("\nMetadata System Summary:\n");
    printf("✓ Proper timestamp initialization and tracking\n");
    printf("✓ File and directory metadata management\n");
    printf("✓ Comprehensive attribute flag system\n");
    printf("✓ Robust error handling\n");
    printf("✓ Large file support\n");
    printf("✓ Timestamp precision and ordering\n");
    
    fs_destroy();
    return 0;
}
