#include "myfs.h"
#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>

const char *MyFs::MYFS_MAGIC = "MYFS";

MyFs::MyFs(BlockDeviceSimulator *blkdevsim_):blkdevsim(blkdevsim_) 
{
	struct myfs_header header;
	blkdevsim->read(0, sizeof(header), (char *)&header);
	

	if (strncmp(header.magic, MYFS_MAGIC, sizeof(header.magic)) != 0 ||
	    (header.version != CURR_VERSION)) 
	{
		std::cout << "Did not find myfs instance on blkdev" << std::endl;
		std::cout << "Creating..." << std::endl;
		format();
		std::cout << "Finished!" << std::endl;
		return;
	}
}

void MyFs::format() {

	// put the header in place
	struct myfs_header header;
	strncpy(header.magic, MYFS_MAGIC, sizeof(header.magic));
	header.version = CURR_VERSION;
	blkdevsim->write(0, sizeof(header), (const char*)&header);


	// Creating inode bitmap to know which inodes are used and which empty
	char inodeBitMap[INODE_NUM] = {0};	
	std::fill_n(inodeBitMap, INODE_NUM, '0');
	inodeBitMap[0] = '1'; // Root folder


	// Writing bits maps to the file
	blkdevsim->write(BLOCK_HEADER_SIZE, sizeof(inodeBitMap), inodeBitMap);

	// Creating first inode for root
	char content[INODE_SIZE];
	struct Inode inode = {0, FOLDER, 0, {START + POINTER_SIZE + BLOCK_SIZE, 0, 0, 0}};
	std::memcpy(content, &inode, sizeof(Inode));
	blkdevsim->write(START + POINTER_SIZE, INODE_SIZE, content);
	

	// Set blocks size (each block have 4 bytes at the beggining that tells how many space is used in that block)
	char addr[POINTER_SIZE] = {'0', '0', '0', '0'};
	for(int blockNum = 1; blockNum < BLOCK_NUM; blockNum++)
	{

		// Adding zeros to address blocks (zero space is used)
		blkdevsim->write(START + BLOCK_SIZE * blockNum, POINTER_SIZE, addr);	
	}
}

void MyFs::create_file(std::string path_str, bool directory) 
{
	int inodeNum;
	char inodeBitMap[INODE_NUM] = {0};
	struct Inode inode;
	char content[INODE_SIZE];
	

	blkdevsim->read(START + POINTER_SIZE, INODE_SIZE, content);
	
	std::memcpy(&inode, content, sizeof(Inode));

	// Reading files that locateed at root dit to see if the file already exsists
	for(int i = 0; i < inode.size / FILE_ENTRY_SIZE; i++)
	{
		char name[FILE_NAME_LEN] = {0};
		blkdevsim->read(inode.addrs[0] + i * FILE_ENTRY_SIZE, FILE_ENTRY_SIZE, content);
		memcpy(name, content, FILE_NAME_LEN);
		if(strcmp(name, path_str.c_str()) == 0)
		{
			return;
		}
	}


	blkdevsim->read(START - INODE_NUM, INODE_NUM, inodeBitMap);		// Read inode bitmap

	for(inodeNum = 0; inodeNum < INODE_NUM; inodeNum++)   // Find free inode
	{
		if(inodeBitMap[inodeNum] == '0')
		{
			inodeBitMap[inodeNum] = '1';
			blkdevsim->write(START - INODE_NUM, INODE_NUM, inodeBitMap);
			break;
		}
	}

	// Writing new inode to the file
	struct Inode newInode = {inodeNum, directory, 0, {0,0,0,0}};
	memcpy(content,  &newInode, INODE_SIZE);
	this->blkdevsim->write(START + POINTER_SIZE + INODE_SIZE * inodeNum, INODE_SIZE, content);

	// Adding file entry to root folder
	char path[FILE_NAME_LEN] = {0};
	char name[FILE_NAME_LEN] = {0};
	strcpy(name, path_str.c_str());
	blkdevsim->write(inode.addrs[0] + inode.size, FILE_NAME_LEN, name);
	char tmp = (char)inodeNum;
	blkdevsim->write(inode.addrs[0] + inode.size + FILE_NAME_LEN, sizeof(char), &tmp);

	// Updating root folder inode
	inode.size += 11;
	memcpy(content, &inode, INODE_SIZE);
	this->blkdevsim->write(START + POINTER_SIZE, INODE_SIZE, content);
}

std::string MyFs::get_content(std::string path_str) 
{
	char fileContent[FILE_ENTRY_SIZE] = {0};
	char name[FILE_NAME_LEN] = {0};

	int size, inodeNum;

	struct Inode rootInode, inode;
	char inodeContent[INODE_SIZE] = {0};
	bool found = false;
	
	// Find root inode
	this->blkdevsim->read(START + POINTER_SIZE, INODE_SIZE, inodeContent);  
	memcpy(&rootInode, inodeContent, INODE_SIZE);

	// Check if file exsists
	for(inodeNum = 0; inodeNum < rootInode.size / FILE_ENTRY_SIZE; inodeNum++)
	{
		blkdevsim->read(rootInode.addrs[0] + inodeNum * FILE_ENTRY_SIZE, FILE_ENTRY_SIZE, fileContent);
		memcpy(name, fileContent, FILE_NAME_LEN);
		if(strcmp(name, path_str.c_str()) == 0) // FIle exists getting its inode
		{
			found = true;
			char n;
			memcpy(&n, fileContent + FILE_NAME_LEN, 1);
			int inodeNumber = (int)n;
			blkdevsim->read(START + POINTER_SIZE + INODE_SIZE * inodeNumber, INODE_SIZE, inodeContent);
			memcpy(&inode, inodeContent, INODE_SIZE);
			break;
		}
	}

	if(!found)
	{
		std::cerr << path_str << ": No such file or directory" << std::endl;
		return "";
	}

	// Reading the content of a file 
	char text[inode.size] = {0};
	blkdevsim->read(POINTER_SIZE + START + BLOCK_SIZE * inode.block + inode.addrs[0], inode.size, text);
	text[inode.size - 1] = '\0';
	return std::string(text);
}


void MyFs::set_content(std::string path_str, std::string content) 
{
	char addr[POINTER_SIZE] = {0};
	char fileContent[FILE_ENTRY_SIZE] = {0};
	char name[FILE_NAME_LEN] = {0};

	int blockNum, size, inodeNum, i_addr;
	bool found = false;

	struct Inode rootInode, inode;
	char inodeContent[INODE_SIZE] = {0};
	int inodeNumber ;

	size = content.size();

	// Block 0 1 already taken
	for(blockNum = 2; blockNum < BLOCK_NUM; blockNum++)
	{

		// reading first 4 bytes that says the next free address and checking if I have enough space
		blkdevsim->read(START + BLOCK_SIZE * blockNum, POINTER_SIZE, addr);	
		std::string tmp(addr);
		i_addr = std::stoi(tmp);
		if(BLOCK_SIZE - POINTER_SIZE - i_addr >= size)
		{

			found = true;
			break;
		}

	}

	// If not fount block exit
	if(!found)
	{
		exit(1);
	}

	// Read / inode to get files in / dir
	this->blkdevsim->read(START + POINTER_SIZE, INODE_SIZE, inodeContent);  
	memcpy(&rootInode, inodeContent, INODE_SIZE);
	int folderSize = rootInode.size;

	found = false;
	// Find file inode
	for(inodeNum = 0; inodeNum < folderSize / FILE_ENTRY_SIZE; inodeNum++)
	{
		blkdevsim->read(rootInode.addrs[0] + inodeNum * FILE_ENTRY_SIZE, FILE_ENTRY_SIZE, fileContent);
		memcpy(name, fileContent, FILE_NAME_LEN);
		if(strcmp(name, path_str.c_str()) == 0)
		{
			found = true;
			char n;
			memcpy(&n, fileContent + FILE_NAME_LEN, 1);
			inodeNumber = (int)n;
			blkdevsim->read(START + POINTER_SIZE + INODE_SIZE * inodeNumber, INODE_SIZE, inodeContent);
			memcpy(&inode, inodeContent, INODE_SIZE);
			break;
		}
	}

	// Check if found inode
	if(!found)
	{
		std::cerr << path_str << ": No such file or directory" << std::endl;
		return;
	}


	// Saving text
	char text[size + 1] = {0};
	strcpy(text, content.c_str());
	blkdevsim->write(POINTER_SIZE  + START + BLOCK_SIZE * blockNum + i_addr, size + 1, text);	

	// Update block size
	i_addr += size;
	std::string tmp = std::to_string(i_addr);
	char tmpAddr[POINTER_SIZE] = {0};
	std::strncpy(tmpAddr, tmp.c_str(), POINTER_SIZE);
	blkdevsim->write(START + BLOCK_SIZE * blockNum, POINTER_SIZE, tmpAddr);	

	if(inode.block > 1 && inode.size && inode.block != blockNum)	// Upadating the previous block size
	{
		blkdevsim->read(START + BLOCK_SIZE * inode.block, POINTER_SIZE, addr);	
		int newSize = std::stoi(addr) - inode.size;
		std::string tmp = std::to_string(newSize);
		strcpy(tmpAddr, tmp.c_str());
		blkdevsim->write(START + BLOCK_SIZE * inode.block, POINTER_SIZE, tmpAddr);	
	}
		
	// Update inode
	inode.size = size;
	inode.addrs[0] = i_addr - size;
	inode.block = blockNum;
	memcpy(inodeContent, &inode, INODE_SIZE);
	blkdevsim->write(START + POINTER_SIZE + INODE_SIZE * inodeNumber, INODE_SIZE, inodeContent);	
}

MyFs::dir_list MyFs::list_dir(std::string path_str) 
{
	dir_list ans;
	char inodeBitMap[INODE_NUM] = {0};
	struct Inode inode;
	char content[INODE_SIZE];
	char fileContent[FILE_ENTRY_SIZE];

	// Read inode of / to get the files that stored in root
	this->blkdevsim->read(START + POINTER_SIZE, INODE_SIZE, content);  
	memcpy(&inode, content, INODE_SIZE);
	int size = inode.size;

	// Going through the / files 
	for(int i = 0; i < size / FILE_ENTRY_SIZE; i++)
	{
		struct dir_list_entry tmp;
		blkdevsim->read(inode.addrs[0] + i * FILE_ENTRY_SIZE, FILE_ENTRY_SIZE, fileContent);
		char n;
		memcpy(&n, fileContent + FILE_NAME_LEN, 1);
		char name[FILE_NAME_LEN] = {0};
		memcpy(name, fileContent, FILE_NAME_LEN);
		int inodeNumber = (int)n;
		struct Inode tmpInode;
		blkdevsim->read(START + POINTER_SIZE + INODE_SIZE * inodeNumber, INODE_SIZE, content);
		memcpy(&tmpInode, content, INODE_SIZE);
		tmp.name = std::string(name);
		tmp.is_dir = tmpInode.type;
		tmp.file_size = tmpInode.size;
		ans.push_back(tmp);
	}
	return ans;
}

