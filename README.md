# Extract/create FTL `.dat` archives

The `*.dat` of FTL files are a concatenation of files.

Warning:

- no security measure on the file creation
  (it might write ../.profile, ../../.profile);

- backup your data;

- backup your save files.

## Usage

~~~sh
# Backup
test data.dat.orig || cp data.dat data.dar.orig
test resources.dat.orig || cp resources.dat data.dar.orig

# Expand
ftldat xt data.dat.orig > data.txt
ftldat xt resources.dat.orig > resources.txt

# TODO, patch the files hereby

# recreate the archive:
ftldat c data.dat $(cat data.txt)
ftldat c resources.dat $(cat resources.txt)
~~~

## Format

All values are in little-endian format (ar least on little-endian machines):

Header:

- 32-bit number file slots;
- 32-bit file offset,
  - this is the offset of the file within the archive file,
  - a value or zero is ignored.

Each file is found at a given offset within the file:

- 32-bit file size;
- 32-bit file name size;
- file name;
- file data.
