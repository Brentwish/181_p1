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


RC PagedFileManager::openFile(const string &fileName, FileHandle &fileHandle)
{
    return -1;
}


RC PagedFileManager::closeFile(FileHandle &fileHandle)
{
    return -1;
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
    return -1;
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
    return -1;
}


RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount)
{
	return -1;
}
