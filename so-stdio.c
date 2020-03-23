// Amazing includes
#include                    <string.h>
#include                    <unistd.h>
#include                    <fcntl.h>
#include                    <sys/types.h>
#include                    <sys/wait.h>
#include                    "so_stdio.h"

// Amazing define
#define IO_BUFFER_SIZE      4096

// Amazing SO structure
typedef struct _so_file
{
    const char* pathName;
    const char* mode;
    
    int fileDescriptor;
    char buffer[IO_BUFFER_SIZE];

    int bufferSize;
    int bufferIndex;

    int errorCode;
    int isEOF;
    char lastOperation;

    int parentPid;
}SO_FILE;

// Returns the file descriptor coresponding to the given pathname and open mode
int GetFileDescriptor(const char *pathname, const char *mode)
{
    if(!strcmp(mode, "r"))
        return open(pathname, O_RDONLY, 0644);
    else if(!strcmp(mode, "r+"))
        return open(pathname, O_RDWR, 0644);
    else if(!strcmp(mode, "w"))
        return open(pathname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    else if(!strcmp(mode, "w+"))
        return open(pathname, O_RDWR | O_CREAT | O_TRUNC, 0644);
    else if(!strcmp(mode, "a"))
        return open(pathname, O_WRONLY | O_CREAT | O_APPEND, 0644);
    else if(!strcmp(mode, "a+"))
        return open(pathname, O_RDWR | O_CREAT | O_APPEND, 0644);

    return SO_EOF;
}

// Zerofies the buffer and resets its values 
void InvalidateBuffer(SO_FILE *stream)
{
    stream->bufferIndex = 0;
    stream->bufferSize = IO_BUFFER_SIZE;
    memset(stream->buffer, 0, stream->bufferSize);
}

void InitializeBufferFlags(SO_FILE *stream)
{
    stream->errorCode = 0;
    stream->isEOF = 0;
    stream->lastOperation = 'n';
}

// Creates a newly initialized SO_FILE with coresponding pathname and open mode
FUNC_DECL_PREFIX SO_FILE *so_fopen(const char *pathname, const char *mode)
{
    SO_FILE *stream;
    
    stream = (SO_FILE*)malloc(1 * sizeof(SO_FILE));
    stream->fileDescriptor = GetFileDescriptor(pathname, mode);

    if(stream->fileDescriptor >= 0)
    {
        stream->pathName = pathname;
        stream->mode = mode;

        InitializeBufferFlags(stream);
        InvalidateBuffer(stream);
        return stream;
    }
    else
    {
        free(stream);
        return NULL;
    }
}

// Flushes the remaining bytes in the buffer and closes the file
FUNC_DECL_PREFIX int so_fclose(SO_FILE *stream)
{
    int flushErrorCode, closeErrorCode;
    char lastOperation;
    
    flushErrorCode = so_fflush(stream);
    closeErrorCode = close(stream->fileDescriptor);
    lastOperation = stream->lastOperation;

    free(stream);
    return lastOperation == 'w' ? flushErrorCode : closeErrorCode;
}

// Returns the file descriptor corresponding to the opened file
FUNC_DECL_PREFIX int so_fileno(SO_FILE *stream)
{
    return stream->fileDescriptor;
}

// Only after a previous write operation flushes the whole buffer
FUNC_DECL_PREFIX int so_fflush(SO_FILE *stream)
{
    int totalNumberOfBytesWritten, currentNumberOfBytesWritten;
    
    totalNumberOfBytesWritten = currentNumberOfBytesWritten = 0;

    if(stream->lastOperation == 'w')
    {
        while(totalNumberOfBytesWritten != stream->bufferIndex)
        {
            currentNumberOfBytesWritten = write
            (
                stream->fileDescriptor, 
                stream->buffer + totalNumberOfBytesWritten, 
                stream->bufferIndex - totalNumberOfBytesWritten
            );

            if(currentNumberOfBytesWritten < 0)
            {
                stream->errorCode = SO_EOF;
                return SO_EOF;
            }
            else
                totalNumberOfBytesWritten += currentNumberOfBytesWritten;
        }

        InvalidateBuffer(stream);
        return 0;
    }

    return SO_EOF;
}

// Discards/Flushes buffer and moves cursor offset bytes from current position
FUNC_DECL_PREFIX int so_fseek(SO_FILE *stream, long offset, int whence)
{
    if(stream->lastOperation == 'r')
        InvalidateBuffer(stream);
    else if(stream->lastOperation == 'w')
        so_fflush(stream);

    return lseek(stream->fileDescriptor, offset, whence) >= 0 ? 0 : SO_EOF;
}

// Returns the current cursor position depending on last read/write operation
FUNC_DECL_PREFIX long so_ftell(SO_FILE *stream)
{
    if(stream->lastOperation == 'r')
        return lseek(stream->fileDescriptor, 0, SEEK_CUR) - stream->bufferSize + stream->bufferIndex;
    else if(stream->lastOperation == 'n' || stream->lastOperation == 'w')
        return lseek(stream->fileDescriptor, 0, SEEK_CUR) + stream->bufferIndex;

    return SO_EOF;
}

// Reads nmemb * size bytes from the buffer using so_fgetc
FUNC_DECL_PREFIX size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    int i, currentReadChar;
    char *derefrencedPtr;
    
    derefrencedPtr = (char*)ptr;

    for (i = 0; i < nmemb * size; i++)
    {
        currentReadChar = so_fgetc(stream);
        
        if(currentReadChar != SO_EOF)
            derefrencedPtr[i] = currentReadChar;
        else
            break;
    }

    return i / size;
}

// Writes nmemb * size bytes to the buffer using so_fputc
FUNC_DECL_PREFIX size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
    int i;
    char *derefrencedPtr;
    
    derefrencedPtr = (char*)ptr;

    for (i = 0; i < nmemb * size; i++)
    {
        if(so_fputc(derefrencedPtr[i], stream) == SO_EOF)
            break;
    }

    return nmemb;
}

// Reads IO_BUFFER_SIZE bytes to the buffer at once and 
// returns the byte available at the bufferIndex
FUNC_DECL_PREFIX int so_fgetc(SO_FILE *stream)
{
    stream->lastOperation = 'r';

    if(stream->bufferIndex == 0 || stream->bufferIndex == stream->bufferSize)
    {
        stream->bufferSize = read(stream->fileDescriptor, stream->buffer, IO_BUFFER_SIZE);
        
        if(stream->bufferSize <= 0)
        {
            if(stream->bufferSize == 0)
                stream->isEOF = 1;
        
            stream->errorCode = SO_EOF;
            return SO_EOF; 
        }
        else
            stream->bufferIndex = 0;
    }

    return (int)((unsigned char)(stream->buffer[(stream->bufferIndex)++]));
}

// Writes the byte given at the bufferIndex and
// flushes the buffer after IO_BUFFER_SIZE bytes
FUNC_DECL_PREFIX int so_fputc(int c, SO_FILE *stream)
{
    stream->lastOperation = 'w';

    if(stream->bufferIndex == stream->bufferSize)
    {
        if(so_fflush(stream) == SO_EOF)
            return SO_EOF;
        else
            InvalidateBuffer(stream);
    }

    stream->buffer[(stream->bufferIndex)++] = c;
    return (int)((unsigned char)(c));
}

// Returns 1 if the end of file was reached and 0 if not
FUNC_DECL_PREFIX int so_feof(SO_FILE *stream)
{
    return stream->isEOF;
}

// Returns 0 if the no error occured until this call or the last error code
FUNC_DECL_PREFIX int so_ferror(SO_FILE *stream)
{
    return stream->errorCode;
}

// Creates one anonymous bidirectional pipe
// In child: close the unwanted ending of the pipe and 
//dup the STDIN/STDOUT to the wanted ending of the pipe
// In parent: close the unwanted ending of the pipe and
// set the wanted ending of the pipe as fileDescriptor for the structure
FUNC_DECL_PREFIX SO_FILE *so_popen(const char *command, const char *type)
{
    SO_FILE *stream;
    pid_t pid;
    int anonymousPipe[2];
    const char *argv[] = {"sh", "-c", command, NULL};

    if (pipe(anonymousPipe) == SO_EOF)
        return NULL;
    
    pid = fork();
    
    // Child process
    if(pid == 0)
    {
        if(!strcmp(type, "r"))
        {
            close(anonymousPipe[0]);
            dup2(anonymousPipe[1], STDOUT_FILENO);
        }
        else if(!strcmp(type, "w"))
        {
            close(anonymousPipe[1]);
            dup2(anonymousPipe[0], STDIN_FILENO);
        }

        execvp("sh", (char *const *) argv);
        exit(SO_EOF);
    }
    // Parent process
    else if(pid > 0)
    {
        stream = (SO_FILE*)malloc(1 * sizeof(SO_FILE));

        close(anonymousPipe[!strcmp(type, "r")]);
        stream->fileDescriptor = anonymousPipe[!strcmp(type, "w")];

        stream->parentPid = pid;

        InitializeBufferFlags(stream);
        InvalidateBuffer(stream);
        return stream;
    }

    return NULL;
}

// Waits for the child to terminate succesfully
FUNC_DECL_PREFIX int so_pclose(SO_FILE *stream)
{
    int pCloseStatus, parentPid;

    parentPid = stream->parentPid;
    so_fclose(stream);
 
    return waitpid(parentPid, &pCloseStatus, 0) != SO_EOF ? pCloseStatus : SO_EOF;
}