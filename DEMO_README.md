# File System Demo Scripts

## Available Demo Files

### 1. `demo_clean.txt` - RECOMMENDED FOR PRESENTATION

**To Run:**
```bash
./fs_shell < demo_clean.txt
```

---

### 2. `demo_quick.txt` - Quick Feature Overview

**To Run::**
```bash
./fs_shell < demo_quick.txt
```

---

### 3. `demo_complete.txt` - Comprehensive Tests

**To Run:**
```bash
./fs_shell < demo_complete.txt
```

---

### 4. `demo_by_feature.txt` - Feature-by-Feature Demo

**To Run:**
```bash
./fs_shell < demo_by_feature.txt
```

## Quick Command Reference

```bash
# Compile
gcc -o fs_shell shell.c fs.c -I. -Wall

# Run interactive shell
./fs_shell

# Run demo script
./fs_shell < demo_clean.txt

# Save output for documentation
./fs_shell < demo_complete.txt > output.log 2>&1

# Run and pause between sections
cat demo_clean.txt | while read line; do 
    echo "fsh> $line"
    echo "$line" | ./fs_shell
    sleep 0.5
done
```