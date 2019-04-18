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
    //Account for the header.
    //null headers don't exist either.
    recordSize += INT_SIZE;
  }

  int slotEntrySize = 2*INT_SIZE;
  return recordSize + slotEntrySize;
}

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
  unsigned int i;
  int j;
  struct Attribute attr;
  int numAttrs = recordDescriptor.size();
  int nullFieldSize = getNullFieldSize(numAttrs);
  char nullField[nullFieldSize];
  memset(nullField, 0, nullFieldSize);
  memcpy(nullField, (char *) data, nullFieldSize);
  //Get size of record
  int recordSize = getRecordSize(recordDescriptor, data);
  cout << "Record size: " << recordSize << endl;

  //Find a page with enough free space to fit the record
  void *page = malloc(PAGE_SIZE);
  memset(page, (int) '\0', PAGE_SIZE);

  //get the right page 
  int found = 0;
  for (i = 0; i < fileHandle.getNumberOfPages(); i++) {
    if (fileHandle.readPage(i, page) != SUCCESS) {
      perror("RecordBasedFileManager - insertRecord(): failed to readPage");
      return ERROR;
    }

    //recordSize currently accounts for the 2 ints that will be added to
    //the slot directory
    if (getFreeSpace(page) >= recordSize) {
      found = 1;
      break;
    }
  }

  //If there were no pages with enough space, create a new formatted one.
  //Should i be incremented again?
  if (!found) {
    newFormattedPage(page);
  }

  //Set the rid
  rid.pageNum = i;
  rid.slotNum = getNumSlots(page) + 1;
  cout << "rid: <" << rid.pageNum << ", " << rid.slotNum << ">\n";

  //write the actual record
  // | numAttrs | nullBytes | f1 | f2 | .. | fn | F1 | F2 | .. | Fn |
  int attr_debug[numAttrs];

  int freeSpaceOffsetVal = getFreeSpaceOffset(page);
  //offset into *data
  char *dataOffset = (char*) data + nullFieldSize;
  //offset to the beginning of freespace
  char *headerOffset = (char*) page + freeSpaceOffsetVal;
  char *attrOffset;

  // numAttrs
  memcpy(headerOffset, &numAttrs, INT_SIZE);
  headerOffset += INT_SIZE;

  //nullBytes
  memcpy(headerOffset, data, nullFieldSize);
  headerOffset += nullFieldSize;

  //f1...fn
  attrOffset = headerOffset + (numAttrs-countNullBits(nullField, nullFieldSize))*INT_SIZE;
  int sizeofAttrs = 0;
  for (j = 0; j < numAttrs; j++) {
    attr = recordDescriptor[j];
    if (checkIfNull(nullField[j/8], j%8)) {
      //Null attrs don't count toward sizeofAttrs
      continue;
    }
    //Copy the value at data offset into the attr offset
    if (attr.type == TypeInt) {
      int d;
      memcpy(&d, dataOffset, INT_SIZE);
      memcpy(attrOffset, &d, INT_SIZE);
      cout << "int: " << d << endl;
      dataOffset += INT_SIZE;
      attrOffset += INT_SIZE;
      sizeofAttrs += INT_SIZE;
    } else if (attr.type == TypeReal) {
      float d;
      memcpy(&d, dataOffset, REAL_SIZE);
      memcpy(attrOffset, &d, REAL_SIZE);
      cout << "real: " << d << endl;
      dataOffset += REAL_SIZE;
      attrOffset += REAL_SIZE;
      sizeofAttrs += REAL_SIZE;
    } else if (attr.type == TypeVarChar) {
      //varchars have an int before the data indicating their size
      int varLen;
      memcpy(&varLen, dataOffset, INT_SIZE);
      dataOffset += INT_SIZE;
      cout << "----varLen: " << varLen << endl;
      char* d = (char*) malloc(varLen+1);
      memset(d, '\0', varLen+1);
      memcpy(d, dataOffset, varLen);
      memcpy(attrOffset, d, varLen);
      cout << "string: " << d << endl;
      dataOffset += varLen;
      attrOffset += varLen;
      sizeofAttrs += varLen;
      free(d);
    } else {
      perror("Error in insertRecord");
      return -1;
    }
    //The header elements get the total size of the counted attrs as its offset
    memcpy(headerOffset, &sizeofAttrs, INT_SIZE);
    attr_debug[j] = sizeofAttrs;
    headerOffset += INT_SIZE;
  }

  for (j = 0; j < numAttrs; j++) {
    cout << "(" << j << ", " << attr_debug[j] << "), ";
  }
  cout << endl;



  void *newSlotDirOffset = (char*) page + getSlotDirOffset(rid.slotNum);
  void *newSlotDirLength = (char*) page + getSlotDirLength(rid.slotNum);
  void *freeSpaceOffset = (char*) page + FREESPACE_OFFSET;
  void *numSlotsOffset = (char*) page + NUM_SLOTS_OFFSET;
  int newNumSlots = getNumSlots(page) + 1;
  //Add a new slot entry in the directory, set it to the location of free space
  memcpy(newSlotDirOffset, &freeSpaceOffsetVal, INT_SIZE);
  memcpy(newSlotDirLength, &recordSize, INT_SIZE);
  //Add 1 to the number of slot entries
  memcpy(numSlotsOffset, &newNumSlots, INT_SIZE);
  //Add the new record's size to the free space pointer
  //need to remove the extra slot we added in the method
  freeSpaceOffsetVal += recordSize - 2*INT_SIZE;
  memcpy(freeSpaceOffset, &freeSpaceOffsetVal, INT_SIZE);


  if (found) {
    fileHandle.writePage(rid.pageNum, page);
  } else {
    fileHandle.appendPage(page);
  }

  free(page);
  return SUCCESS;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    //get the file pointer
    FILE* fp = fileHandle.getFileHandler();
    //create a new empty page to load page into
    void *page = malloc(PAGE_SIZE);
    memset(page, (int) '\0', PAGE_SIZE);
    //get the right page
    int found = 0;
    int pNum = rid.pageNum;  
    if (fileHandle.readPage(pNum, page) != SUCCESS) {
        perror("RecordBasedFileManager - insertRecord(): failed to readPage");
        return ERROR;
    }

    //get the right slot info
    // char* slotAddr = (char*) page + NUM_SLOTS_OFFSET - 4 - (rid.slotNum - 1)* 2 * INT_SIZE; // length and offset
    char* slotAddr = (char* ) getSlotDirOffset(rid.slotNum);
    getSlotDirLength(rid.slotNum);
    //get the offset of the record
    char* recOffset;
    memcpy(recOffset, slotAddr, INT_SIZE);

    // need the size to read the whole record in
    int recLen = getSlotDirLength(rid.slotNum);

    // memcpy(&recLen, (char*) slotAddr - 4, INT_SIZE);
    // recLen = (char*) page - recOffset;

    // get the record 
    // when we read need to subtract the len of record from the offset 
    fillData(page, recordDescriptor, data, recOffset, recLen);

    free(page);
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

//For a given page, get the amount of space available
int RecordBasedFileManager::getFreeSpace(void *page) {
  int freeSpace = PAGE_SIZE - getSlotDirectorySize(page) - getFreeSpaceOffset(page);
  cout << "Free Space: " << freeSpace << endl;
  return freeSpace;
}

//Calculate the number of null bytes for a number of attributes
int RecordBasedFileManager::getNullFieldSize(int numAttrs) {
    return ceil((double) numAttrs/8);
}

//For a given page, return the number of slots.
int RecordBasedFileManager::getNumSlots(void *page) {
  int numSlots;
  // NUM_SLOTS_OFFSET refers to exact address of it 
  memcpy(&numSlots, (char *)page + NUM_SLOTS_OFFSET, INT_SIZE);
  return numSlots;
}

//Return the offset to free space
int RecordBasedFileManager::getFreeSpaceOffset(void *page) {
  int freeSpaceOffset;
  memcpy(&freeSpaceOffset, (char *)page + FREESPACE_OFFSET, INT_SIZE);
  cout << "getFreeSpaceOffset = " << freeSpaceOffset << endl;
  return freeSpaceOffset;
}

//Calculate relative offset to a given slot dir entry (offset)
int RecordBasedFileManager::getSlotDirOffset(int j) {
  return PAGE_SIZE - (2*j + 1)*INT_SIZE;
}

//Calculate relative offset to a given slot dir entry (length)
int RecordBasedFileManager::getSlotDirLength(int j) {
  return PAGE_SIZE - (2*j + 2)*INT_SIZE;
}

int RecordBasedFileManager::getSlotDirectorySize(void* page) {
  //2*getNumSlots(page) for each (offset, length) pair)
  //+2 for the free_space offset and num_slots ints
  return (2*getNumSlots(page) +2)*INT_SIZE;
}

void RecordBasedFileManager::newFormattedPage (void* page) {
	//add the slot directory header 
	//do the free page offset first
	int slotNum = 0; 
  memset(page, (int) '\0', PAGE_SIZE);
	memcpy((char*) page + NUM_SLOTS_OFFSET, &slotNum, INT_SIZE);
	//make the free page offset at the top of the file
	int freeSpaceOffset = 0;
	memcpy((char*) page + FREESPACE_OFFSET, &freeSpaceOffset, INT_SIZE);
}

void RecordBasedFileManager::fillData(void* page, const vector<Attribute> &recordDescriptor,const void* data, char* offset, int recLen) 
{
	// get the size of the record 
	cout << recLen << "\n";
	// subtract the size of the record to get to the front of the record
	// char* start = (char* ) page - recLen;
	// get the null fields 

	// fill the null fields bits in data

	// for field in recordDescriptor
		// check if field is null
			// if null 
				// don't move data offset
				// move the headeroffset and valueoffset pointer? 
				// continue

		// if type == TypeInt
			// write next 4 bytes in data and increase data offset
			// decrease headeroffset by 4 
			// decrease valueoffset by 4

		// if type == TypeReal
			// write next 4 bytes in data and increase data offset
			// decrease headeroffset by 4
			// decrease valueoffset by 4

		// if type == TypeVarChar
			// get the len of the varChar
			// write len to the next 4 bytes of data and increase data offset by 4
			// decrease valueoffset by 4

			// copy the varChar from page to data 
			// increase dataOffset by varCharLen
			// decrease headerffset by 4
			// decrease valueoffset by varCharLen

	// should be done?



}

int RecordBasedFileManager::countNullBits(char nullField[], int size) {
  char byte;
  int numBits = 0;
  for (int i = 0; i < size; i++) {
    byte = nullField[i];
    for (int j = 0; j < 8; j++) {
      if (checkIfNull(byte, j))
        numBits += 1;
    }
  }
  cout << "Num Null bits: " << numBits << endl;
  return numBits;
}
