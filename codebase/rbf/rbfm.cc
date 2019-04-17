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

RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const void *data, RID &rid) {
    return -1;
}

RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const vector<Attribute> &recordDescriptor, const RID &rid, void *data) {
    return -1;
}

RC RecordBasedFileManager::printRecord(const vector<Attribute> &recordDescriptor, const void *data) {
    int offset = 0;
    // get the null bytes first 
    int numBytes = ceil((double) recordDescriptor.size()/8);
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

int RecordBasedFileManager::checkIfNull(char c, int n) {
    static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((c & mask[n]) != 0);
}