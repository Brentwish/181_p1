#include "rbfm.h"

RecordBasedFileManager* RecordBasedFileManager::_rbf_manager = 0;
PagedFileManager* RecordBasedFileManager::_pf_manager = 0;

RecordBasedFileManager* RecordBasedFileManager::instance()
{
    if(!_rbf_manager)
        _rbf_manager = new RecordBasedFileManager();
	
    return _rbf_manager;
}

RecordBasedFileManager::RecordBasedFileManager()
{
}

RecordBasedFileManager::~RecordBasedFileManager()
{
}

RC RecordBasedFileManager::createFile(const string &fileName) {
	return _pf_manager->createFile(fileName);
}

RC RecordBasedFileManager::destroyFile(const string &fileName) {
    return _pf_manager->destroyFile(fileName);
}

RC RecordBasedFileManager::openFile(const string &fileName, FileHandle &fileHandle) {
    return _pf_manager->openFile(fileName, fileHandle);
}

RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
    return _pf_manager->closeFile(fileHandle);
}

int RecordBasedFileManager::getRecordSize(const vector<Attribute> &recordDescriptor, const void *data) {
  // a record consists of:
  // | numAttrs | nullBytes | f1 | f2 | .. | fn | f | F1 | F2 | .. | Fn |
  
  struct Attribute attr;
  int numAttrs = recordDescriptor.size();
  int nullFieldSize = getNullFieldSize(numAttrs);
  char nullField[nullFieldSize];
  memset(nullField, 0, nullFieldSize);
  memcpy(nullField, (char *) data, nullFieldSize);

  int attrOffset = nullFieldSize;

  int recordSize = 0;
  // add | numAttrs | nullBytes | size
  recordSize += sizeof(int) + nullFieldSize;
  // add | f1 | f2 | .. | fn | f |
  recordSize += (numAttrs + 1) * sizeof(int);

  //This for loop gets the | F1 | F2 | .. | Fn | size
  for (int i = 0; i < numAttrs; i++) {
    attr = recordDescriptor[i];
    if (checkIfNull(nullField[i/8], i%8)) {
      continue;
    }
    if (attr.type == TypeInt) {
      recordSize += INT_SIZE;
      attrOffset += INT_SIZE;
    } else if (attr.type == TypeReal) {
      recordSize += REAL_SIZE;
      attrOffset += REAL_SIZE;
    } else if (attr.type == TypeVarChar) {
      //varchars have an int before the data indicating their size
      int varLen;
      memcpy(&varLen, ((char*) data + attrOffset), sizeof(int));
      attrOffset += sizeof(int);
      attrOffset += varLen;
      recordSize += varLen;
    } else {
      perror("Error in getRecordSize");
      return -1;
    }
  }

  return recordSize;
}


int RecordBasedFileManager::getFreeSpace(void *page) {
  int freeSpace, freeSpaceOffset, numSlots;
  freeSpaceOffset = getFreeSpaceOffset(page);
  numSlots = getNumSlots(page);
  cout << "FreeSpaceOffset: " << freeSpaceOffset << endl;
  cout << "numSlots: " << numSlots << endl;
  freeSpace = PAGE_SIZE - ((numSlots + 2) * INT_SIZE) - freeSpaceOffset;
  cout << "Free Space: " << freeSpace << endl;
  return freeSpace;
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
  unsigned int i;
  //Get size of record
  int recordSize = getRecordSize(recordDescriptor, data);
  cout << "Record size: " << recordSize << endl;

  //Find a page with enough free space to fit the record
  void *page = malloc(PAGE_SIZE);
  memset(page, (int) '\0', PAGE_SIZE);

  int found = 0;
  for (i = 0; i < fileHandle.getNumberOfPages(); i++) {
    if (fileHandle.readPage(i, page) != SUCCESS) {
      perror("RecordBasedFileManager - insertRecord(): failed to readPage");
      return ERROR;
    }

    if (getFreeSpace(page) >= recordSize) {
      found = 1;
      break;
    }
  }

  //If there were no pages with enough space, create a new one.
  //Should i be incremented again?
  if (!found) {
    memset(page, (int) '\0', PAGE_SIZE);
    fileHandle.appendPage(page);
  }

  //Set the rid
  rid.pageNum = i;
  rid.slotNum = getNumSlots(page) + 1;
  cout << "rid: <" << rid.pageNum << ", " << rid.slotNum << ">\n";

  void *newSlotDirOffset = (char*) page + getSlotDirOffset(rid.slotNum);
  void *freeSpaceOffset = (char*) page + FREESPACE_OFFSET;
  void *numSlotsOffset = (char*) page + NUM_SLOTS_OFFSET;
  int freeSpaceOffsetVal = getFreeSpaceOffset(page);
  int newNumSlots = getNumSlots(page) + 1;

  //Add a new slot entry in the directory, set it to the location of free space
  memcpy(newSlotDirOffset, &freeSpaceOffsetVal, INT_SIZE);
  //Add the new record's size to the free space pointer
  freeSpaceOffsetVal += recordSize;
  memcpy(freeSpaceOffset, &freeSpaceOffsetVal, INT_SIZE);
  //Add 1 to the number of slot entries
  memcpy(numSlotsOffset, &newNumSlots, INT_SIZE);

  fileHandle.writePage(rid.pageNum, page);

  free(page);
  return SUCCESS;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    return -1;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    int offset = 0;
    // get the null bytes first 
    //int numBytes = ceil((double) recordDescriptor.size()/8);
    int numBytes = getNullFieldSize(recordDescriptor.size());
    cout << numBytes << "- NumBytes \n";
    offset += numBytes;
    
    // Load into an array
    char nullArray[numBytes];
    memcpy(nullArray, data, numBytes);



    struct Attribute A;
    for(int i=0; i<recordDescriptor.size(); i++) {
        A = recordDescriptor[i];

        // check if the record is null 
        if (checkIfNull(nullArray[i/8], i%8)) {
            cout << A.name << ": NULL ";
            continue;
        }

        if (A.type == TypeInt) {
            // offset += 4; 
            int aInt;
            memcpy(&aInt, ((char*) data + offset), sizeof(int));
            offset += 4;
            cout << A.name << ": " << aInt << " "; 
        }
        else if (A.type == TypeReal) {
            float aReal;
            memcpy(&aReal, ((char*) data+ offset), sizeof(int));
            offset += 4;
            cout << A.name << ": " << aReal << " ";
        }
        else if (A.type == TypeVarChar) {
            int varLen;
            memcpy(&varLen, ((char*) data + offset), sizeof(int));
            offset += 4;
            // cout << varLen << "- VarLen\n";

            char* varString = (char*) malloc(varLen + 1);
            memcpy(varString, ((char*) data + offset), varLen);
            cout << A.name << ": " << varString << " ";
            offset += varLen;
            free (varString);
        }
    }
    cout << "\n";
    return SUCCESS;
}

//Given the null byte c, determine if the slot n is null
int RecordBasedFileManager::checkIfNull(char c, int n) {
    static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((c & mask[n]) != 0);
}

//Calculate the number of null bytes for a number of attributes
int RecordBasedFileManager::getNullFieldSize(int numAttrs) {
    return ceil((double) numAttrs/8);
}

//For a given page, return the number of slots.
int RecordBasedFileManager::getNumSlots(void *page) {
  int numSlots;
  memcpy(&numSlots, (char *) page + NUM_SLOTS_OFFSET, INT_SIZE);
  return numSlots;
}

//Return the offset to free space
int RecordBasedFileManager::getFreeSpaceOffset(void *page) {
  int freeSpaceOffset;
  memcpy(&freeSpaceOffset, (char *) page + FREESPACE_OFFSET, INT_SIZE);
  return freeSpaceOffset;
}

//Calculate relative offset to a given slot dir entry
int RecordBasedFileManager::getSlotDirOffset(int j) {
  return PAGE_SIZE - (j + 2)*INT_SIZE;
}
