#include "const.h"
#include "transplant.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 *
 * IMPORTANT: You MAY NOT use floating point arithmetic or declare
 * any "float" or "double" variables.  IF YOU VIOLATE THIS RESTRICTION,
 * YOU WILL GET A ZERO!
 */

/*
 * A function that returns printable names for the record types, for use in
 * generating debugging printout.
 */
static char *record_type_name(int i) {
    switch(i) {
    case START_OF_TRANSMISSION:
    return "START_OF_TRANSMISSION";
    case END_OF_TRANSMISSION:
    return "END_OF_TRANSMISSION";
    case START_OF_DIRECTORY:
    return "START_OF_DIRECTORY";
    case END_OF_DIRECTORY:
    return "END_OF_DIRECTORY";
    case DIRECTORY_ENTRY:
    return "DIRECTORY_ENTRY";
    case FILE_DATA:
    return "FILE_DATA";
    default:
    return "UNKNOWN";
    }
}

int compareStr (char *p1, char *p2)
{
    char *s1 = p1;
    char *s2 = p2;
    char c1, c2;
    do
    {
      c1 = *s1++;
      c2 = *s2++;
      if (c1 == '\0')
        return !(c1 - c2);
    } while (c1 == c2);
  return !(c1 - c2);
}

int str_length(char* ptr) {
    int i = 0;
    for (char* p = ptr; *p; p++)
    {
        i++;
    }
    return i;
}

/*
 * @brief  Initialize path_buf to a specified base path.
 * @details  This function copies its null-terminated argument string into
 * path_buf, including its terminating null byte.
 * The function fails if the argument string, including the terminating
 * null byte, is longer than the size of path_buf.  The path_length variable
 * is set to the length of the string in path_buf, not including the terminating
 * null byte.
 *
 * @param  Pathname to be copied into path_buf.
 * @return 0 on success, -1 in case of error
 */
int path_init(char *name) {
    int i = 0;
    char *s = name;
    for (; *s; s++)
    {
        if ((i+2) >= PATH_MAX)
        {
            return -1;
        }
        *(path_buf+i) = *s;
        i++;
    }
    *(path_buf+i) = *s;
    path_length = i;
    return 0;
}

/*
 * @brief  Append an additional component to the end of the pathname in path_buf.
 * @details  This function assumes that path_buf has been initialized to a valid
 * string.  It appends to the existing string the path separator character '/',
 * followed by the string given as argument, including its terminating null byte.
 * The length of the new string, including the terminating null byte, must be
 * no more than the size of path_buf.  The variable path_length is updated to
 * remain consistent with the length of the string in path_buf.
 *
 * @param  The string to be appended to the path in path_buf.  The string must
 * not contain any occurrences of the path separator character '/'.
 * @return 0 in case of success, -1 otherwise.
 */
int path_push(char *name) {
    int length_path = str_length(path_buf);
    debug("legnth of %s is %d", path_buf, length_path);
    *(path_buf+length_path) = '/';
    int i = 1;
    char* p = name;
    for (; *p; p++,i++)
    {
        *(path_buf+i+length_path) = *p;
    }
    *(path_buf+i+length_path) = *p;
    path_length += i;
    debug("now path_buf has become %s, path_length: %d", path_buf, path_length);
    return 1;
}

/*
 * @brief  Remove the last component from the end of the pathname.
 * @details  This function assumes that path_buf contains a non-empty string.
 * It removes the suffix of this string that starts at the last occurrence
 * of the path separator character '/'.  If there is no such occurrence,
 * then the entire string is removed, leaving an empty string in path_buf.
 * The variable path_length is updated to remain consistent with the length
 * of the string in path_buf.  The function fails if path_buf is originally
 * empty, so that there is no path component to be removed.
 *
 * @return 0 in case of success, -1 otherwise.
 */
int path_pop() {
    if (!(*path_buf))
    {
        return -1;
    }

    debug("---> path_buf: %s", path_buf);
    int i = str_length(path_buf)-1;
    while (*(path_buf+i) != '/' && i) {
        *(path_buf+i) = *(path_buf+i+1);
        debug("===> path_buf: %s", path_buf);
        path_length--;
        i--;
    }
    if (*(path_buf+i) == '/')
    {
        *(path_buf+i) = *(path_buf+i+1);
    }
    debug("finish popping: %s", path_buf);
    return 0;
}

int verify_magic_header() {
    int ch1 = getchar();
    int ch2 = getchar();
    int ch3 = getchar();
    int res = ch1==MAGIC0 && ch2==MAGIC1 && ch3==MAGIC2;
    if (!res)
    {
        debug("Incorrect Magic Header(hex): %x, %x, %x", ch1, ch2, ch3);
    }
    return res;
}

off_t get_n_chars(int n) {
    off_t res = 0;
    while (n--) {
        res = (res << 8) + getchar();
    }
    return res;
}

int verify_depth(int exp_depth) {
    off_t record_depth = get_n_chars(4);
    if (record_depth != exp_depth) {
        debug("Incorrect depth of %ld, expected %d\n", record_depth, exp_depth);
        return 0;
    }
    return 1;
}

off_t get_record_size() {
    return get_n_chars(8);
}

int get_record_type() {
    return getchar();
}

int file_exist (char *filename)
{
  struct stat buffer;
  return (stat (filename, &buffer) == 0);
}

int clobber() {
    return global_options & 0x08;
}

/*
 * @brief Deserialize directory contents into an existing directory.
 * @details  This function assumes that path_buf contains the name of an existing
 * directory.  It reads (from the standard input) a sequence of DIRECTORY_ENTRY
 * records bracketed by a START_OF_DIRECTORY and END_OF_DIRECTORY record at the
 * same depth and it recreates the entries, leaving the deserialized files and
 * directories within the directory named by path_buf.
 *
 * @param depth  The value of the depth field that is expected to be found in
 * each of the records processed.
 * @return 0 in case of success, -1 in case of an error.  A variety of errors
 * can occur, including depth fields in the records read that do not match the
 * expected value, the records to be processed to not being with START_OF_DIRECTORY
 * or end with END_OF_DIRECTORY, or an I/O error occurs either while reading
 * the records from the standard input or in creating deserialized files and
 * directories.
 */
int deserialize_directory(int depth) {
    if (!verify_magic_header())     // magic header
    {
        debug("incorrect magic header for dir");
        return -1;
    }
    int record_type = get_record_type();    // record type
    if (!verify_depth(depth))           // depth
    {
        return -1;
    }
    off_t record_size = get_record_size();      // record size
    debug("record size is %ld", record_size);
    if (record_type == DIRECTORY_ENTRY)
    {
        debug("record type is %x, into DIRECTORY_ENTRY", record_type);
        mode_t mode = get_n_chars(4);       // metadata.st_mode
        get_n_chars(8);     // metadata.st_size
        off_t file_name_size = record_size-HEADER_SIZE-12;
        debug("mode is %x", mode);
        debug("file_name_size is %ld", file_name_size);
        
        debug("before pushing name_buf: %s", path_buf);
        for (int i = 0; i < file_name_size; i++)
        {
            char ch = getchar();
            *(name_buf+i) = ch;
            debug("the %d ch of filename is %x", i, ch);
        }
        debug("now name_buf is %s", name_buf);
        path_push(name_buf);
        for (int i = 0; i < file_name_size; i++)
        {
            *(name_buf+i) = '\0';
        }
        debug("after pushing name_buf: %s", path_buf);

        // following the entry is either file or dir, create
        if (deserialize_file(depth) == -1)
        {
            debug("it isn't a file");
            mkdir(path_buf, mode);
            deserialize_directory(depth+1);
        } else {
            deserialize_directory(depth);
        }
    } else if (record_type == START_OF_DIRECTORY)
    {
        // do nothing, because this dir must've been created when dir_entry is traversed
        deserialize_directory(depth);
    } else if (record_type == END_OF_DIRECTORY)
    {
        deserialize_directory(depth-1);
        path_pop();
    }
    return 0;
}

/*
 * @brief Deserialize the contents of a single file.
 * @details  This function assumes that path_buf contains the name of a file
 * to be deserialized.  The file must not already exist, unless the ``clobber''
 * bit is set in the global_options variable.  It reads (from the standard input)
 * a single FILE_DATA record containing the file content and it recreates the file
 * from the content.
 *
 * @param depth  The value of the depth field that is expected to be found in
 * the FILE_DATA record.
 * @return 0 in case of success, -1 in case of an error.  A variety of errors
 * can occur, including a depth field in the FILE_DATA record that does not match
 * the expected value, the record read is not a FILE_DATA record, the file to
 * be created already exists, or an I/O error occurs either while reading
 * the FILE_DATA record from the standard input or while re-creating the
 * deserialized file.
 */
int deserialize_file(int depth) {
    if (file_exist(path_buf) && !clobber()) // file exists & clobber
    {
        debug("file exists and not clobber");
        return -1;
    }
    if (!verify_magic_header())     // magic header
    {
        debug("incorrect magic header for file");
        return -1;
    }
    int record_type = get_record_type();   // record_type
    int verify_depth_res = verify_depth(depth); // depth
    off_t record_size = get_record_size();     // data size
    if (FILE_DATA != record_type)
    {
        debug("Incorrect record type of %d:%s, expected: FILE_DATA\n",
         record_type, record_type_name(record_type));
        return -1;
    }

    if (!verify_depth_res) { // depth
        debug("incorrect depth of file");
        return -1;
    }
    //"w": If the file exists, its contents are overwritten.
    //     If the file does not exist, it will be created.
    FILE * fPtr;
    fPtr = fopen(path_buf, "w");
    if(fPtr == NULL)
    {
        debug("Unable to create file of %s\n", path_buf);
        return -1;
    }
    debug("create file %s success, starts to put data", path_buf);

    for (off_t i = 0; i < record_size-HEADER_SIZE; i++)
    {
        char ch = getchar();
        fputc(ch, fPtr);
        debug("put char of %c to file %s", ch, path_buf);
    }

    fclose(fPtr);
    debug("deserialize file of %s success, depth %d", path_buf, depth);
    path_pop();
    return 0;
}

void put_n_bytes_from_int(int num, int n) {
    if (n > 1)
    {
        put_n_bytes_from_int(num >> 8, n-1);
    }
    putchar(num);
}

void put_n_bytes(off_t num, int n) {
    if (n > 1)
    {
        put_n_bytes(num >> 8, n-1);
    }
    putchar(num);
}

int record_header(int record_type, int depth, off_t size) {
    putchar(MAGIC0);
    putchar(MAGIC1);
    putchar(MAGIC2);
    putchar(record_type);
    put_n_bytes_from_int(depth, 4);
    put_n_bytes(size, 8);
    return 0;
}

int need_ignore(char *dir_name) {
    if (compareStr(dir_name, ".") || compareStr(dir_name, ".."))
    {
        debug("ignore this sub file");
        return 1;
    }
    return 0;
}

/*
 * @brief  Serialize the contents of a directory as a sequence of records written
 * to the standard output.
 * @details  This function assumes that path_buf contains the name of an existing
 * directory to be serialized.  It serializes the contents of that directory as a
 * sequence of records that begins with a START_OF_DIRECTORY record, ends with an
 * END_OF_DIRECTORY record, and with the intervening records all of type DIRECTORY_ENTRY.
 *
 * @param depth  The value of the depth field that is expected to occur in the
 * START_OF_DIRECTORY, DIRECTORY_ENTRY, and END_OF_DIRECTORY records processed.
 * Note that this depth pertains only to the "top-level" records in the sequence:
 * DIRECTORY_ENTRY records may be recursively followed by similar sequence of
 * records describing sub-directories at a greater depth.
 * @return 0 in case of success, -1 otherwise.  A variety of errors can occur,
 * including failure to open files, failure to traverse directories, and I/O errors
 * that occur while reading file content and writing to standard output.
 */
int serialize_directory(int depth) {
    DIR *dir = NULL;
    debug("start to open file of %s", path_buf);
    if ((dir = opendir(path_buf)) == NULL)
    {
        return -1;
    }
    debug("open file of %s success", path_buf);
    record_header(START_OF_DIRECTORY, depth, HEADER_SIZE);  // start of dir
    debug("reocrd START_OF_DIRECTORY success, depth: %d", depth);

    struct dirent *dp;
    while ((dp = readdir (dir)) != NULL) {
        char *dir_name = dp->d_name;
        if (need_ignore(dir_name))
        {
            continue;
        }
        
        path_push(dir_name);
        // dir entry header (12 for the fixed size of metadata)
        int file_name_length = str_length(dir_name);
        debug("length for %s is %d", dir_name, file_name_length);
        int size = HEADER_SIZE + 12 + file_name_length;
        record_header(DIRECTORY_ENTRY, depth, size);
        debug("record DIRECTORY_ENTRY success, depth: %d, size: (hex)%x",
             depth, size);

        // metadata
        struct stat stat_buf;
        stat(path_buf, &stat_buf);
        put_n_bytes_from_int(stat_buf.st_mode, 4);
        put_n_bytes(stat_buf.st_size, 8);

        // filename
        debug("putting filename");
        for (char* ptr = dir_name; *ptr; ptr++)
        {
            putchar(*ptr);
            debug("putchar(%c), (hex)%x", *ptr, *ptr);
        }

        if (S_ISREG(stat_buf.st_mode))  // regular file
        {
            debug("it's a file: %s", path_buf);
            serialize_file(depth, stat_buf.st_size);
        } else
        {
            debug("it's a dir: %s", path_buf);
            serialize_directory(depth+1);   // directory
        }
        path_pop();
    }

    record_header(END_OF_DIRECTORY, depth, HEADER_SIZE);    // end of dir
    fflush(stdout);
    closedir(dir);
    return 0;
}

/*
 * @brief  Serialize the contents of a file as a single record written to the
 * standard output.
 * @details  This function assumes that path_buf contains the name of an existing
 * file to be serialized.  It serializes the contents of that file as a single
 * FILE_DATA record emitted to the standard output.
 *
 * @param depth  The value to be used in the depth field of the FILE_DATA record.
 * @param size  The number of bytes of data in the file to be serialized.
 * @return 0 in case of success, -1 otherwise.  A variety of errors can occur,
 * including failure to open the file, too many or not enough data bytes read
 * from the file, and I/O errors reading the file data or writing to standard output.
 */
int serialize_file(int depth, off_t size) {
    if (!size)
    {
        return 0;
    }

    record_header(FILE_DATA, depth, size+HEADER_SIZE);
    debug("record header if FILE_DATA success, depth %d, size %ld, record size %ld", 
        depth, size, size+HEADER_SIZE);

    FILE *fp = NULL;
    if (!(fp = fopen(path_buf, "r"))){
        debug("Failed to load file");
        return -1;
    }

    while (size--)
    {
        char c = fgetc(fp);
        if (feof(fp)) // Checking for end of file
            break ;
        putchar(c);
        debug("putchar(%c)", c);
    }

    fclose(fp);
    debug("fp close success");
    return 0;
}

/**
 * @brief Serializes a tree of files and directories, writes
 * serialized data to standard output.
 * @details This function assumes path_buf has been initialized with the pathname
 * of a directory whose contents are to be serialized.  It traverses the tree of
 * files and directories contained in this directory (not including the directory
 * itself) and it emits on the standard output a sequence of bytes from which the
 * tree can be reconstructed.  Options that modify the behavior are obtained from
 * the global_options variable.
 *
 * @return 0 if serialization completes without error, -1 if an error occurs.
 */
int serialize() {
    DIR *dir = NULL;
    debug("start to open file of %s", path_buf);
    if ((dir = opendir(path_buf)) == NULL)
    {
        debug("open file for %s failed\n", path_buf);
        return -1;
    }

    record_header(START_OF_TRANSMISSION, 0, HEADER_SIZE);  // start of transmission
    debug("record starting header of transmission success");
    serialize_directory(1);

    record_header(END_OF_TRANSMISSION, 0, HEADER_SIZE);  // end of transmission
    closedir(dir);
    return 0;
}

/**
 * @brief Reads serialized data from the standard input and reconstructs from it
 * a tree of files and directories.
 * @details  This function assumes path_buf has been initialized with the pathname
 * of a directory into which a tree of files and directories is to be placed.
 * If the directory does not already exist, it is created.  The function then reads
 * from from the standard input a sequence of bytes that represent a serialized tree
 * of files and directories in the format written by serialize() and it reconstructs
 * the tree within the specified directory.  Options that modify the behavior are
 * obtained from the global_options variable.
 *
 * @return 0 if deserialization completes without error, -1 if an error occurs.
 */
int deserialize() {
    if (!verify_magic_header())     // magic header
    {
        debug("incorrect magic header for root");
        return -1;
    }
    int record_type = get_record_type();    // record type
    if (!verify_depth(0))           // depth
    {
        return -1;
    }
    get_record_size();      // record size
    if (record_type != START_OF_TRANSMISSION)
    {
        return -1;
    }

    // first start of directory
    if (!verify_magic_header())     // magic header
    {
        return -1;
    }
    record_type = get_record_type();    // record type
    if (!verify_depth(1))           // depth
    {
        return -1;
    }
    get_record_size();      // record size
    if (record_type != START_OF_DIRECTORY)
    {
        return -1;
    }
    mkdir(path_buf, 0700);
    debug("making file of %s success", path_buf);
    deserialize_directory(1);
    return 0;
}

int valid_clobber(char **ptr)
{
    if (compareStr(*ptr, "-c"))
    {
        global_options = global_options | 0x08;
        return 1;
    }
    return 0;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variable "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int validargs(int argc, char **argv)
{
    if (argc <= 1)
    {
        return -1;
    }
    for (int i = 0; i < argc; ++i)
    {
        debug("%s\n", *(argv+i));
    }
    global_options = 0x00;
    char* first = *(argv+1);
    if (compareStr(first, "-h"))
    {
        debug("starts with h");
        global_options = global_options | 0x01;
        return 0;
    }
    if (compareStr(first, "-s"))
    {
        debug("starts with s");
        global_options = global_options | 0x02;
        // -s
        if (argc == 2)
        {
            return 0;
        }
        // -s -p xxx
        if (argc == 4 && compareStr(*(argv+2), "-p"))
        {
            path_init(*(argv+3));
            return 0;
        }
        

    } else if (compareStr(first, "-d"))
    {
        debug("starts with d, argc is %d", argc);
        global_options = global_options | 0x04;
        if (argc == 2 || (argc == 3 && valid_clobber(argv+2)))  // -d or -d -c
        {
            return 0;
        }
        // -d -c -p xx
        if (argc == 5 && valid_clobber(argv+2) && compareStr(*(argv+3), "-p"))
        {
            path_init(*(argv+4));
            return 0;
        }
        // -d -p xx [-c]
        if (argc >= 4 && compareStr(*(argv+2), "-p") && (argc < 5 || valid_clobber(argv+4))) 
        {
            path_init(*(argv+3));
            return 0;            
        }

    }
    global_options = 0x00;
    return -1;
}