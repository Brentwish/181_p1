#include "pfm.h"

PagedFileManager* PagedFileManager::_pf_manager = 0;

PagedFileManager* PagedFileManager::instance()
{
    if(!_pf_manager)
        _pf_manager = new PagedFileManager();

    return _pf_manager;
}


PagedFileManager::PagedFileManager()
{
}


PagedFileManager::~PagedFileManager()
{
}

// return if the file exists
bool checkFile(const string &fileName) {
	return ( access( fileName.c_str(), F_OK ) != -1 );
}

RC PagedFileManager::createFile(const string &fileName)
{
    // make sure the file does not exist beforehand
    if (checkFile(fileName) ) return 1;
    FILE* fp = fopen(fileName.c_str(), "wb+");
    //FileHandle.setFileHandler(fp);
    fclose(fp);
    return 0;
}


RC PagedFileManager::destroyFile(const string &fileName)
{
    // if file DNE 
    if (!checkFile(fileName) ) return 2;    
    return remove(fileName.c_str());
}

// open the file and set the fileHandler to it 
RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
    //check if the file exists 
    if (!checkFile(fileName) ) return 2;

    //check if the file handle has a file open
    // if (fileHandle.getFileHandler() != NULL) return 3;

    FILE* fp = fopen(fileName.c_str(), "wrb+");

    fileHandle.setFileHandler(fp);
    return 0;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    // get the file from the fileHandle
    FILE* fp = fileHandle.fd;

    // set the new fd for the filehandler to be null
    fileHandle.setFileHandler(NULL);
    fclose(fp);
    return 0;
}


FileHandle::FileHandle()
{
	readPageCounter = 0;
	writePageCounter = 0;
	appendPageCounter = 0;
}

FILE* FileHandle::getFileHandler() {
	return this->fd;
}

void FileHandle::setFileHandler(FILE* f) {
	this->fd = f;
}

FileHandle::~FileHandle()
{
}


RC FileHandle::readPage(PageNum pageNum, void *data)
{
    FILE* fp = getFileHandler(); // get the file handler
    
    //set the position in the stream
    fseek(fp, PAGE_SIZE * pageNum - 1, SEEK_SET);
    
    // copy the bits into data 
    memcpy(data, (void*) SEEK_CUR, PAGE_SIZE);
    this->readPageCounter = this->readPageCounter + 1;
    return 0;

}


RC FileHandle::writePage(PageNum pageNum, const void *data)
{
    return -1;
}


RC FileHandle::appendPage(const void *data)
{
    return -1;
}


unsigned FileHandle::getNumberOfPages()
{
    return 0;
}


RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
	readPageCount = this->readPageCounter;
	writePageCount = this->writePageCounter;
	appendPageCount = this->appendPageCounter;

	return 0;
}
