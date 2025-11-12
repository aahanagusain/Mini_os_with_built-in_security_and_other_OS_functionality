# Mini OS Quick Reference Card

## Core Commands

### System
```
help              Show all commands
about             About the OS
clear             Clear screen
history           Show command history
whoami            Current user info
```

### User Management
```
adduser <name>    Create new user (password prompted)
deluser <name>    Delete user (root only)
su <name>         Switch to user (password prompted)
users             List all users
logout            Logout current user
```

### File Operations
```
touch <path>      Create empty file
mkdir <path>      Create directory
rm <path>         Delete file
edit <path>       View file contents
cat <path>        Display file with paging
echo "text" > <f> Write text to file
write <f> <text>  Write to file
cp <src> <dst>    Copy file
mv <src> <dst>    Rename file
```

### File Information
```
ls [path]         List files with metadata
stat <path>       Show file statistics
pwd               Print working directory
chmod <m> <f>     Change permissions
chown <u:g> <f>   Change owner
```

### Security & Network
```
fw list                 Show firewall rules
fw allow port <N>       Allow port
fw deny port <N>        Block port
fw allow ip <A.B.C.D>   Allow IP
fw deny ip <A.B.C.D>    Block IP
```

### Cryptography & Math
```
sha256("text")    SHA-256 hash
sha224("text")    SHA-224 hash
encrypt <s> <d> <k> Encrypt file (XOR cipher)
decrypt <s> <d> <k> Decrypt file (XOR cipher)
compress <s> <d>   Compress file (RLE)
decompress <s> <d> Decompress file (RLE)
math              Math functions list
crypto            Crypto utilities list
```

### Time
```
datetime          Current date and time
date              Current date
clock             Current time
```

### System Control
```
reboot            Restart system
shutdown          Shutdown system
```

---

## User Creation Workflow

```bash
# Step 1: Create user
adduser alice
Enter password: ****

# Step 2: List users
users

# Step 3: Switch to user
su alice
Password: ****

# Step 4: Verify
whoami

# Step 5: Logout
logout
```

---

## File Management Workflow

```bash
# Create files
touch myfile.txt
mkdir mydir
echo "content" > file.txt

# View files
ls
cat file.txt
stat myfile.txt
edit myfile.txt

# Modify permissions
chmod 755 myfile.txt
chown 1:1 myfile.txt

# Manage files
cp myfile.txt backup.txt
mv myfile.txt renamed.txt
rm myfile.txt
```

---

## Firewall Configuration

```bash
# Allow services
fw allow port 22      # SSH
fw allow port 80      # HTTP
fw allow port 443     # HTTPS

# Block services
fw deny port 23       # Telnet
fw deny port 25       # SMTP (limit)

# Allow networks
fw allow ip 192.168.1.0
fw allow ip 10.0.0.0

# Block hosts
fw deny ip 192.168.1.100

# Inspect rules
fw list
```

---

## Tips & Tricks

### Security
- Root user has uid=0
- Passwords stored as SHA-256 hashes
- File ownership tracked with uid:gid
- Firewall default-deny policy

### Files
- Files created with current user as owner
- Permissions shown in ls output (rwx format)
- Overlay system allows modifying packaged files

### Performance
- Commands are case-insensitive
- Arguments preserve case
- Buffer overflow protection enabled
- Max 16 users, 256 firewall rules

### Navigation
- Single-level filesystem (no subdirectories)
- All files accessible from root /
- cd command limited in current version
- pwd shows / always

---

## Error Handling

| Command | Error | Solution |
|---------|-------|----------|
| adduser | User exists | Choose different name |
| adduser | User DB full | Delete user or reboot |
| su | Auth failed | Wrong password |
| deluser | Can't delete self | Logout first |
| fw | Rules full | Remove old rules |
| touch | Permission denied | Run as appropriate user |

---

## File Permissions Format

```
-rw-r--r-- owner 1:1 256 bytes filename
│││││││││  │     │ │ │
│││││││││  │     │ │ └─ Size
│││││││││  │     │ └─ Group ID
│││││││││  │     └─ User ID  
│││││││││  └─ Owner name
└┬┴┬┴┬┴┬┘
 │ │ │ │
 │ │ │ └─ Others (x=execute, w=write, r=read)
 │ │ └─ Group permissions
 │ └─ Owner permissions
 └─ File type (- = file, d = directory)
```

Octal notation: `chmod 755 file` = `rwxr-xr-x`

---

## Building & Running

### Build
```bash
cd /path/to/os
make pegasus.iso
```

### Run
```bash
# QEMU
qemu-system-i386 -cdrom pegasus.iso

# With options
qemu-system-i386 -cdrom pegasus.iso -m 512 -display gtk
```

### Boot
- Wait for login prompt
- Enter username and password
- Or press Enter for guest access
- Start using commands

---

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Enter | Execute command |
| Backspace | Delete character |
| Shift+Key | Uppercase (with shift) |
| Capslock | Toggle caps |
| Space | Pager continue (in cat) |
| q | Pager quit (in cat) |

---

## Encryption & Compression Workflows

### File Encryption
```bash
# Encrypt a file with XOR cipher
encrypt secret.txt secret.txt.enc mykey

# Decrypt the file
decrypt secret.txt.enc secret.txt.dec mykey

# View encrypted file (binary)
edit secret.txt.enc

# View decrypted file
edit secret.txt.dec
```

### File Compression
```bash
# Compress a file with RLE
compress document.txt document.txt.rle

# Check file sizes
stat document.txt
stat document.txt.rle

# Decompress the file
decompress document.txt.rle document.txt.dec

# Verify contents match
edit document.txt
edit document.txt.dec
```

### Best Practices
1. **Encryption**: Use strong keys (16+ characters) mixing letters, numbers, symbols
2. **Compression**: Best for repetitive text (logs, code with blank lines)
3. **Combined**: Encrypt THEN compress for maximum security
4. **Backup**: Always keep originals until verified encryption/compression works

---

## Environment

| Variable | Value |
|----------|-------|
| Max Users | 16 |
| Max Rules | 256 |
| Max Encryption Key | 256 chars |
| Max File Size (Op) | 65KB |
| Max RLE Run | 255 bytes |
| User Name | 32 chars |
| Password | 64 chars (hex) |
| Buffer Size | 1024 chars |
| Command Size | 1024 chars |

---

## Help Resources

- `help` - Show all commands
- `help` followed by command name - Specific help (future)
- `FEATURES_ADDED.md` - Detailed documentation
- `DEMO_GUIDE.md` - Usage examples
- `IMPLEMENTATION_SUMMARY.md` - Technical details

---

**Version:** 1.1  
**Build:** November 12, 2025  
**Architecture:** 32-bit x86
