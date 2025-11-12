# Features Added to Mini OS

## Overview
This document outlines all the new security, user management, and file management features added to the Mini OS kernel.

---

## 1. User Management System

### Commands

#### `whoami`
Shows the currently logged-in user with their UID.
```
> whoami
Current user: alice (uid:1)
```

#### `users`
Lists all registered users in the system with their UIDs and GIDs.
```
> users
Registered users:
  * root (uid:0 gid:0)
  * alice (uid:1 gid:1)
  * bob (uid:2 gid:2)
```

#### `adduser <username>`
Creates a new user account with password authentication.
- Prompts for password (masked with asterisks)
- Stores passwords as SHA-256 hashes
- Automatically assigns UID/GID
```
> adduser alice
Enter password: ****
User created successfully
```

#### `deluser <username>` (Root Only)
Deletes a user account. Only root can delete users.
```
> deluser alice
User deleted successfully
```

#### `su <username>`
Switches to another user account with password verification.
```
> su alice
Password: ****
Switched to user: alice
```

#### `logout`
Logs out the current user, returning to guest mode.
```
> logout
Logged out successfully
```

### User API Functions

- `user_init_from_file()` - Load users from /etc/passwd
- `user_login()` - Authenticate with username/password
- `user_current()` - Get currently logged-in user
- `user_logout()` - Log out
- `user_add()` - Add new user with hashed password
- `user_switch()` - Switch to different user
- `user_is_root()` - Check if current user is root (uid=0)
- `user_get_by_name()` - Lookup user by name
- `user_list_all()` - List all users
- `user_delete()` - Remove user account

---

## 2. File Management System

### Commands

#### `touch <path>`
Creates an empty file or updates timestamp. Assigns current user as owner.
```
> touch myfile.txt
Created: myfile.txt
```

#### `mkdir <path>`
Creates a new directory. Assigns current user as owner.
```
> mkdir /home/alice
Directory created: /home/alice
```

#### `echo <text> > <file>`
Writes text to a file. Creates file if it doesn't exist.
```
> echo "Hello World" > greeting.txt
Written to: greeting.txt
```

#### `edit <path>`
Views file contents in read-only mode (view only).
```
> edit greeting.txt
--- File: greeting.txt (size: 11) ---
Hello World
--- (View only mode) ---
```

#### `rm <path>`
Removes/deletes a file.
```
> rm myfile.txt
Removed: myfile.txt
```

#### `stat <path>`
Displays detailed file statistics including size, owner, permissions, and type.
```
> stat myfile.txt
File: myfile.txt
  Size: 256 bytes
  Owner: uid:1 gid:1
  Mode: 644
  Type: file
```

#### `pwd`
Prints current working directory (limited to root "/" in current implementation).
```
> pwd
/
```

#### `cd <path>`
Change directory (limited in single-level filesystem).
```
> cd /home
(cd not implemented in single-level filesystem)
```

#### `write <path> <text>`
Write text to file with overlay support for packaged files.
```
> write config.txt "option=value"
(write) wrote 15 bytes to config.txt
```

#### `cp <src> <dst>`
Copy file from source to destination.
```
> cp file1.txt file1_backup.txt
(cp) file1.txt -> file1_backup.txt
```

#### `ls [path]`
List directory contents with detailed metadata.
```
> ls /
drwx------ root 0:0 4096 bytes
-rw-r--r-- alice 1:1 256 bytes file.txt
```

### File Operations Features

- **Permissions**: Display and modify via `chmod`/`chown` commands
- **Ownership**: Track uid/gid for each file
- **Overlay Support**: Package files can be modified via overlay system
- **Metadata**: Size, mode, owner information
- **Type Detection**: Directory vs. regular file identification

---

## 3. Network Security & Firewall

### Commands

#### `fw list`
Display all active firewall rules.
```
> fw list
Firewall Rules:
0: DENY ANY
1: ALLOW PORT 80
2: ALLOW IP 192.168.1.0/255.255.255.0
```

#### `fw allow port <N>`
Allow inbound traffic on specific port.
```
> fw allow port 80
Added rule to allow port 80
```

#### `fw deny port <N>`
Block inbound traffic on specific port.
```
> fw deny port 23
Added rule to deny port 23
```

#### `fw allow ip <A.B.C.D>`
Allow traffic from specific IP address.
```
> fw allow ip 10.0.0.1
Added rule to allow IP 10.0.0.1
```

#### `fw deny ip <A.B.C.D>`
Block traffic from specific IP address.
```
> fw deny ip 192.168.1.100
Added rule to deny IP 192.168.1.100
```

### Firewall Features

- **Default-Deny Policy**: All traffic blocked unless explicitly allowed
- **Rule Matching**: Match traffic by port or IP address
- **Rule Priority**: Rules checked in order of addition
- **Rule Types**: ALLOW and DENY rules
- **Target Types**: ANY traffic, specific PORT, or specific ADDRESS
- **Network Masking**: Support for CIDR notation (implicit /32 for single IPs)

### Firewall API

- `netsec_init()` - Initialize firewall with default rules
- `netsec_add_rule()` - Add new firewall rule
- `netsec_remove_rule()` - Remove rule by index
- `netsec_list_rules()` - Get all current rules
- `netsec_check_connection()` - Validate connection against rules

---

## 4. Enhanced Security Features

### Password Hashing
- SHA-256 based password hashing
- Automatic detection of hex-digest format
- Backwards compatibility with plaintext passwords

### Privilege Escalation
- `sudo <command>` - Execute command as root (root users only)
- Root UID = 0
- Permission checks integrated with filesystem

### File Permissions
- POSIX-style permission bits (mode field)
- Owner UID/GID tracking
- `chmod <mode> <path>` - Change permissions
- `chown <uid>:<gid> <path>` - Change ownership

---

## 5. Command Enhancements

### Case Handling
- Commands are case-insensitive
- Arguments (filenames, usernames) preserve case
- Allows case-sensitive user/file handling

### Safety Improvements
- Buffer overflow protection with size checks
- Input validation on file operations
- Error handling for permission denied scenarios
- File descriptor leak prevention

---

## Architecture

### User Subsystem
- Max 16 users in memory
- User database loaded from /etc/passwd
- Current user tracking
- Dynamic user management

### File System
- RAMFS (RAM-based filesystem)
- Metadata support (uid/gid/mode)
- Overlay system for modifications
- Directory and file support

### Network Security
- Max 256 firewall rules
- In-memory rule storage
- Connection validation framework
- Ready for network integration

---

## Usage Examples

### Create a New User
```
> adduser developer
Enter password: ****
User created successfully

> users
Registered users:
  * root (uid:0 gid:0)
  developer (uid:1 gid:1)
```

### Switch User and Create Files
```
> su developer
Password: ****
Switched to user: developer

> touch project.txt
Created: project.txt

> echo "int main() {}" > main.c
Written to: main.c

> ls
-rw-r--r-- developer 1:1 13 bytes project.txt
-rw-r--r-- developer 1:1 14 bytes main.c
```

### Set Up Firewall
```
> fw allow port 22
Added rule to allow port 22

> fw allow port 80
Added rule to allow port 80

> fw deny ip 192.168.1.50
Added rule to deny IP 192.168.1.50

> fw list
Firewall Rules:
0: DENY ANY
1: ALLOW PORT 22
2: ALLOW PORT 80
3: DENY IP 192.168.1.50/255.255.255.255
```

---

## Technical Details

### Memory Management
- Dynamic allocation for user/rule storage
- Bounds checking to prevent buffer overflows
- Automatic cleanup on deletion

### Error Handling
- Comprehensive error codes
- User-friendly error messages
- Graceful degradation

### Performance
- O(n) user lookup (n ≤ 16)
- O(n) firewall rule matching (n ≤ 256)
- Efficient string operations

---

## Future Enhancements

1. **User Groups**: Support for group-based permissions
2. **ACLs**: Fine-grained access control lists
3. **Audit Logging**: Track security events
4. **Rate Limiting**: DDoS protection
5. **Encryption**: File encryption support
6. **Multi-level Directories**: Full hierarchical filesystem
7. **Process Isolation**: User-based process separation
8. **Network Stack Integration**: Actual network firewall enforcement

---

## Building and Testing

```bash
# Build the ISO
make pegasus.iso

# Run in QEMU
qemu-system-i386 -cdrom pegasus.iso

# Boot and test commands
qemu> help
qemu> adduser alice
qemu> users
qemu> fw allow port 80
```

---

## Files Modified

- `include/user.h` - Extended user management API
- `src/user.c` - User subsystem implementation
- `src/kernel.c` - Shell command handlers
- `include/netsec.h` - Firewall API
- `src/netsec.c` - Firewall implementation
- `src/memory.c` - Fixed memcpy signature
- `include/encrypt.h` - Encryption API
- `src/encrypt.c` - Encryption implementation (XOR cipher)
- `include/compress.h` - Compression API
- `src/compress.c` - Compression implementation (RLE)

---

## 5. File Encryption and Decryption

### Implementation
- **Algorithm**: XOR cipher with key-based encryption
- **Security**: Educational implementation; for production use stronger encryption like AES
- **Key Length**: Supports keys up to 256 characters
- **File Size**: Handles files up to 65KB per operation

### API Functions

#### `encrypt_xor(input, input_len, output, output_len, key, key_len)`
Performs XOR encryption on data with key cycling.
- **Parameters**:
  - `input`: Source data buffer
  - `input_len`: Length of source data
  - `output`: Destination buffer for encrypted data
  - `output_len`: Size of output buffer
  - `key`: Encryption key string
  - `key_len`: Length of key
- **Returns**: Length of encrypted data or error code

#### `decrypt_xor(input, input_len, output, output_len, key, key_len)`
Decrypts XOR-encrypted data (same operation as encryption).
- **Returns**: Length of decrypted data or error code

#### `encrypt_file(src_path, dst_path, key)`
Encrypts an entire file using the specified key.
- **Parameters**:
  - `src_path`: Source file path
  - `dst_path`: Destination file path for encrypted data
  - `key`: Encryption key
- **Returns**: Number of bytes encrypted or error code
- **Error Codes**:
  - `-1`: Invalid parameters
  - `-2`: Invalid key length
  - `-3`: Cannot open source file
  - `-4`: Cannot create destination file
  - `-5`: Read error
  - `-6`: Encryption error
  - `-7`: Write error

#### `decrypt_file(src_path, dst_path, key)`
Decrypts an encrypted file.
- **Parameters**: Same as encrypt_file
- **Returns**: Number of bytes decrypted or error code

### Commands

#### `encrypt <srcfile> <dstfile> <key>`
Encrypts a file using XOR cipher.
```
> encrypt document.txt document.txt.enc secretkey
File encrypted successfully: 1024 bytes

> edit document.txt.enc
--- File: document.txt.enc ---
ß@H∆ÿ±ò├╬▌└·«┤├∞∙─°±┤...
```

#### `decrypt <srcfile> <dstfile> <key>`
Decrypts a previously encrypted file.
```
> decrypt document.txt.enc document.txt.dec secretkey
File decrypted successfully: 1024 bytes

> edit document.txt.dec
--- File: document.txt.dec ---
This is my secret document that was encrypted!
```

---

## 6. File Compression and Decompression

### Implementation
- **Algorithm**: Run-Length Encoding (RLE)
- **Benefits**: Excellent compression for repetitive data; moderate for general text
- **Expansion Risk**: Data with few repetitions may expand slightly
- **Maximum Run**: 255 bytes per run (configurable)
- **File Size**: Handles files up to 65KB per operation

### API Functions

#### `compress_rle(input, input_len, output, output_len)`
Compresses data using RLE algorithm.
- **Format**: `[byte_value][count] [byte_value][count] ...`
- **Parameters**:
  - `input`: Source data
  - `input_len`: Length of source data
  - `output`: Destination for compressed data
  - `output_len`: Size of output buffer
- **Returns**: Length of compressed data or error code
- **Error Codes**:
  - `-1`: Invalid parameters
  - `-2`: Output buffer too small
  - `-3`: Encoding error

#### `decompress_rle(input, input_len, output, output_len)`
Decompresses RLE-encoded data.
- **Returns**: Length of decompressed data or error code
- **Error Codes**:
  - `-1`: Invalid parameters
  - `-2`: Invalid compressed format

#### `compress_file(src_path, dst_path)`
Compresses an entire file using RLE.
- **Parameters**:
  - `src_path`: Source file path
  - `dst_path`: Destination file path for compressed data
- **Returns**: Number of bytes written or error code

#### `decompress_file(src_path, dst_path)`
Decompresses a previously compressed file.
- **Parameters**: Same as compress_file
- **Returns**: Number of bytes written or error code

### Commands

#### `compress <srcfile> <dstfile>`
Compresses a file using RLE algorithm.
```
> compress data.txt data.txt.rle
File compressed successfully: 512 bytes

> stat data.txt
--- File Statistics: data.txt ---
Size: 1024 bytes
Owner: alice (uid:1)
Mode: 0644

> stat data.txt.rle
--- File Statistics: data.txt.rle ---
Size: 512 bytes
Owner: alice (uid:1)
Mode: 0644
```

#### `decompress <srcfile> <dstfile>`
Decompresses a previously compressed file.
```
> decompress data.txt.rle data.txt.dec
File decompressed successfully: 1024 bytes

> edit data.txt.dec
--- File: data.txt.dec ---
[Original content restored]
```

### Compression Examples

**Best Case** (highly repetitive data):
```
Input:  AAAAAABBBBCCCCCCCCDDEE (22 bytes)
Output: A6B4C8D2E2 (10 bytes)
Ratio:  45% compression
```

**Worst Case** (random data):
```
Input:  ABCDEFGHIJKLMNOP (16 bytes)
Output: A1B1C1D1...O1 (32 bytes)
Ratio:  200% expansion
```

---

## Credits

Mini OS Security and User Management Implementation
