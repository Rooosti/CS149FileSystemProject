# File System Implementation - Complete Feature Set

## ✅ All Required Features Implemented

### 1. Create Files with Name and Content ✅
- **Functions:** `create_file()`, `write_file()`
- **Commands:** `create PATH`, `write PATH DATA`
- **Example:**
  ```bash
  fsh> create /home/test.txt
  fsh> write /home/test.txt Hello, World!
  ```

### 2. Open and Close Files ✅ **NEW**
- **Functions:** `fs_open()`, `fs_close()`
- **Commands:** `open PATH MODE`, `close FD`
- **Example:**
  ```bash
  fsh> open /home/test.txt r
  File opened with descriptor: 0
  fsh> close 0
  Successfully closed file
  ```

### 3. Read and Write Data ✅
- **Path-based:**
  - Functions: `read_file()`, `write_file()`
  - Commands: `read PATH`, `write PATH DATA`
- **Descriptor-based:** ✅ **NEW**
  - Functions: `fs_read_fd()`, `fs_write_fd()`, `fs_seek()`
  - Commands: `readfd FD`, `writefd FD DATA`, `seek FD OFFSET`
- **Example:**
  ```bash
  fsh> write test.txt Some data
  fsh> read test.txt
  Some data
  
  fsh> open test.txt rw
  File opened with descriptor: 0
  fsh> writefd 0 More data
  Wrote 9 bytes
  fsh> seek 0 0 0
  New position: 0
  fsh> readfd 0
  Some dataMore data
  fsh> close 0
  ```

### 4. Delete and Rename Files ✅
- **Delete:**
  - Functions: `rm_file()`, `rmdir_empty()`
  - Commands: `rm PATH`, `rmdir PATH`
- **Rename:** ✅ **NEW**
  - Function: `rename_file()`
  - Commands: `rename OLD NEW`, `mv OLD NEW`
- **Example:**
  ```bash
  fsh> rename test.txt newname.txt
  Successfully renamed/moved file
  fsh> mv newname.txt /backup/file.txt
  Successfully renamed/moved file
  fsh> rm /backup/file.txt
  Successfully removed file!
  ```

### 5. List Directory Contents ✅
- **Function:** `ls_dir()`
- **Command:** `ls [PATH]`
- **Example:**
  ```bash
  fsh> ls /home
  documents/
  downloads/
  test.txt
  ```

### 6. Search for Files by Name ✅
- **Function:** `fs_search()`
- **Command:** `search TERM`
- **Example:**
  ```bash
  fsh> search test
  /home/test.txt
  /backup/test.bak
  2 matches found
  ```

### 7. Save and Load Filesystem to/from Disk ✅ **NEW**
- **Functions:** `fs_save()`, `fs_load()`
- **Commands:** `save DISKFILE`, `load DISKFILE`
- **Example:**
  ```bash
  fsh> save myfilesystem.dat
  Successfully saved filesystem
  fsh> exit
  
  # Later, restart the program:
  fsh> load myfilesystem.dat
  Successfully loaded filesystem
  fsh> ls /
  # All your files are back!
  ```

## Compilation and Usage

### Compile:
```bash
gcc -o fs_shell shell.c fs.c -I. -Wall -Wextra -std=c99
```

### Run:
```bash
./fs_shell
```

## Complete Command Reference

### File & Directory Management
| Command | Description | Example |
|---------|-------------|---------|
| `mkdir PATH` | Create directory | `mkdir /home/user` |
| `ls [PATH]` | List directory | `ls /home` |
| `create PATH` | Create empty file | `create test.txt` |
| `cd PATH` | Change directory | `cd /home` |
| `rm PATH` | Remove file | `rm test.txt` |
| `rmdir PATH` | Remove empty directory | `rmdir /tmp` |
| `rename OLD NEW` | Rename/move file | `rename a.txt b.txt` |
| `mv OLD NEW` | Alias for rename | `mv a.txt /backup/a.txt` |

### File I/O (Path-based)
| Command | Description | Example |
|---------|-------------|---------|
| `write PATH TEXT` | Write text to file | `write file.txt Hello` |
| `read PATH` | Read file contents | `read file.txt` |

### File I/O (Descriptor-based) **NEW**
| Command | Description | Example |
|---------|-------------|---------|
| `open PATH MODE` | Open file (r/w/rw) | `open file.txt r` |
| `close FD` | Close file descriptor | `close 0` |
| `readfd FD [LEN]` | Read from descriptor | `readfd 0 100` |
| `writefd FD TEXT` | Write to descriptor | `writefd 0 data` |
| `seek FD OFFSET [W]` | Seek in file | `seek 0 10 0` |

### Metadata & Search
| Command | Description | Example |
|---------|-------------|---------|
| `info PATH` | Show file metadata | `info test.txt` |
| `attr PATH FLAGS` | Set attributes | `attr test.txt 2` |
| `touch PATH` | Update timestamps | `touch test.txt` |
| `search TERM` | Find matching files | `search .txt` |

### Persistence **NEW**
| Command | Description | Example |
|---------|-------------|---------|
| `save DISKFILE` | Save filesystem | `save fs.dat` |
| `load DISKFILE` | Load filesystem | `load fs.dat` |

### System
| Command | Description |
|---------|-------------|
| `help` | Show help |
| `exit` | Quit shell |

## Advanced Features

### File Descriptors
The file descriptor system allows you to:
- Open multiple files simultaneously (up to 32)
- Read/write at specific positions using seek
- Maintain position across multiple read/write operations
- Use different access modes (read-only, write-only, read-write)

**Example workflow:**
```bash
fsh> create log.txt
fsh> open log.txt w
File opened with descriptor: 0
fsh> writefd 0 [INFO] System started
fsh> writefd 0 [INFO] Processing data
fsh> close 0

fsh> open log.txt r
File opened with descriptor: 1
fsh> readfd 1
[INFO] System started[INFO] Processing data
fsh> close 1
```

### Persistence
The filesystem can be saved to disk and restored:
- All files, directories, and metadata are preserved
- File contents are saved
- Timestamps and attributes are maintained
- Works across program restarts

**Example workflow:**
```bash
# Session 1
fsh> mkdir /data
fsh> create /data/important.txt
fsh> write /data/important.txt Critical information
fsh> save backup.dat
fsh> exit

# Session 2 (after restart)
fsh> load backup.dat
fsh> read /data/important.txt
Critical information
```

### File Attributes
Files and directories can have attributes:
- `0` - None
- `1` - Hidden
- `2` - Read-only
- `4` - System
- `8` - Archive

Combine attributes with addition:
```bash
fsh> attr file.txt 3    # Hidden + Read-only (1+2)
fsh> attr file.txt 0    # Clear all attributes
```

## Testing

A comprehensive test file is provided: `test_new_features.txt`

Run through the commands to test all functionality:
```bash
./fs_shell < test_new_features.txt
```

Or manually copy commands from the test file.

## Architecture

### Data Structures
- **Hierarchical tree:** Files and directories organized in a tree
- **Linear search:** O(n) lookup within directories
- **File descriptors:** Array-based FD table for open files
- **Metadata:** Timestamps (created/modified/accessed) and attributes

### Key Components
1. **Node system:** Tree structure with parent/child pointers
2. **Path resolution:** Walk function for path traversal
3. **Dynamic allocation:** Files grow as needed
4. **Persistence layer:** Text-based serialization format

## All Requirements Met ✅

| Requirement | Status | Implementation |
|-------------|--------|----------------|
| Create files with name/content | ✅ | `create_file()` + `write_file()` |
| Open and close files | ✅ | `fs_open()` + `fs_close()` |
| Read and write data | ✅ | Path-based + descriptor-based I/O |
| Delete files | ✅ | `rm_file()` |
| Rename files | ✅ | `rename_file()` |
| List directory | ✅ | `ls_dir()` |
| Search by name | ✅ | `fs_search()` |
| Save/load to disk | ✅ | `fs_save()` + `fs_load()` |

## Future Enhancements (Optional)

- Hash table for faster lookups
- B-tree for disk-based storage
- File permissions system
- Symbolic links
- File locking
- Recursive directory removal
- Wildcard pattern matching

---

**Author:** CS149 File System Project  
**Date:** December 2025  
**Version:** 2.0 (Complete Implementation)
