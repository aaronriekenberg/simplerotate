#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <unistd.h>

#define DEBUG_ENABLED 0
#define LOCK_FILE_NAME "lock"
#define OUTPUT_FILE_NAME "output"
#define MAX_OUTPUT_FILE_SIZE_BYTES (10 * 1024 * 1024)
#define MAX_OUTPUT_FILES (10)

#define DEBUG_PRINTF(fmt, ...) do { if (DEBUG_ENABLED) printf(fmt, ##__VA_ARGS__); } while (0)

static void acquireLock() {
  int fd;
  int retVal;

  DEBUG_PRINTF("acquireLock\n");

  fd = open(LOCK_FILE_NAME, O_APPEND | O_CREAT | O_WRONLY, 0644);
  if (fd < 0) {
    DEBUG_PRINTF("error opening lock errno = %s\n", strerror(errno));
    exit(1);
  }

  retVal = flock(fd, LOCK_EX);
  if (retVal < 0) {
    DEBUG_PRINTF("error on flock errno = %s\n", strerror(errno));
    exit(1);
  }

  DEBUG_PRINTF("acquired LOCK_EX lock on fd %d\n", fd);
}

static size_t getOutputFileBytes() {
  off_t bytes = 0;
  struct stat s;

  if (stat(OUTPUT_FILE_NAME, &s) == 0) {
    bytes = s.st_size;
  }

  return bytes;
}

struct RotationInfo {
  char* fromFilename;
  char* toFilename;
  SIMPLEQ_ENTRY(RotationInfo) entry;
};

SIMPLEQ_HEAD(RotationInfoList, RotationInfo);

struct RotationInfoList* rotationInfoList;

static char* buildRotationFileName(int fileIndex) {
  char* s;

  if (fileIndex == 0) {
    s = strdup(OUTPUT_FILE_NAME);
  } else {
    asprintf(&s, "%s.%d", OUTPUT_FILE_NAME, fileIndex);
  }

  return s;
}

static void buildRotationInfoList() {
  int i;
  struct RotationInfo* rotationInfo;

  DEBUG_PRINTF("in buildRotationInfoList\n");
  rotationInfoList = calloc(1, sizeof(struct RotationInfoList));
  SIMPLEQ_INIT(rotationInfoList);

  if (MAX_OUTPUT_FILES <= 1) {
    return;
  }

  for (i = MAX_OUTPUT_FILES - 1; i > 0; --i) {
    rotationInfo = calloc(1, sizeof(struct RotationInfo));
    rotationInfo->fromFilename = buildRotationFileName(i - 1);
    rotationInfo->toFilename = buildRotationFileName(i);
    SIMPLEQ_INSERT_TAIL(
      rotationInfoList,
      rotationInfo,
      entry);
    rotationInfo = NULL;
  }

  if (DEBUG_ENABLED) {
    i = 0;
    SIMPLEQ_FOREACH(
      rotationInfo,
      rotationInfoList,
      entry) {
      DEBUG_PRINTF(
        "rotationInfo[%d] = { fromFilename = %s, toFilename = %s }\n",
        i, rotationInfo->fromFilename, rotationInfo->toFilename);
      ++i;
    }
  }

  DEBUG_PRINTF("end buildRotationFileName\n");
}

static void rotateFiles() {
  const struct RotationInfo* rotationInfo;

  SIMPLEQ_FOREACH(
    rotationInfo,
    rotationInfoList,
    entry) {
    DEBUG_PRINTF(
      "rename fromFilename = %s, toFilename = %s\n",
      rotationInfo->fromFilename, rotationInfo->toFilename);

    rename(rotationInfo->fromFilename, rotationInfo->toFilename);
  }
}

int main(int argc, char** argv) {
  char buffer[8192];
  int outputFD;
  ssize_t readRetVal;
  ssize_t writeRetVal;
  size_t bytesWritten;
  int intRetVal;
  size_t outputFileSize;

  buildRotationInfoList();

  if (argc > 1) {
    DEBUG_PRINTF("chdir %s\n", argv[1]);
    intRetVal = chdir(argv[1]);
    if (intRetVal < 0) {
      DEBUG_PRINTF("chdir failed errno = %s\n", strerror(errno));
      exit(1);
    }
  }

  acquireLock();

  outputFileSize = getOutputFileBytes();
  DEBUG_PRINTF("initial outputFileSize = %zu\n", outputFileSize);

  outputFD = open(OUTPUT_FILE_NAME, O_APPEND | O_CREAT | O_WRONLY, 0644);
  if (outputFD < 0) {
    DEBUG_PRINTF("error opening output errno = %s\n", strerror(errno));
    exit(1);
  }

  while (true) {
    readRetVal = read(STDIN_FILENO, buffer, sizeof(buffer));
    if (readRetVal == 0) {
      DEBUG_PRINTF("read returned 0\n");
      exit(0);
    } else if (readRetVal < 0) {
      DEBUG_PRINTF("read returned < 0 errno = %s\n", strerror(errno));
      exit(1);
    }

    DEBUG_PRINTF("read returned %zd\n", readRetVal);
    bytesWritten = 0;
    do {
      writeRetVal = write(outputFD, buffer + bytesWritten, readRetVal);
      if (writeRetVal < 0) {
        DEBUG_PRINTF("write returned < 0 errno = %s\n", strerror(errno));
        exit(1);
      }

      DEBUG_PRINTF("write returned %zd\n", writeRetVal);
      readRetVal -= writeRetVal; 
      bytesWritten += writeRetVal;
      outputFileSize += writeRetVal;

      if (outputFileSize >= MAX_OUTPUT_FILE_SIZE_BYTES) {
        intRetVal = close(outputFD);
        if (intRetVal < 0) {
          DEBUG_PRINTF("close returned < 0 errno = %s\n", strerror(errno));
          exit(1);
        }

        rotateFiles();       
        outputFileSize = 0;

        outputFD = open(OUTPUT_FILE_NAME, O_TRUNC | O_CREAT | O_WRONLY, 0644);
        if (outputFD < 0) {
          DEBUG_PRINTF("error opening output errno = %s\n", strerror(errno));
          exit(1);
        }
      }
    } while (readRetVal > 0);
  }

  return 0;
}
