#ifndef __MYFS_H__
#define __MYFS_H__

#include <memory>
#include <vector>
#include <stdint.h>
#include "blkdev.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <regex>

#define FILE_NAME_LEN 10
#define BLOCK_SIZE 4100
#define INODE_SIZE 32
#define INODE_NUM 132
#define BLOCK_HEADER_SIZE 5
#define POINTER_SIZE 4
#define NUM_OF_ADDRESS 5
#define BLOCK_NUM 132
#define START 137
#define FOLDER 1
#define FILE 0
#define FILE_ENTRY_SIZE 11


class MyFs {
public:
	MyFs(BlockDeviceSimulator *blkdevsim_);

	/**
	 * dir_list_entry struct
	 * This struct is used by list_dir method to return directory entry
	 * information.
	 */
	struct dir_list_entry {
		/**
		 * The directory entry name
		 */
		std::string name;
		/**
		 * whether the entry is a file or a directory
		 */
		bool is_dir;

		/**
		 * File size
		 */
		int file_size;
	};
	typedef std::vector<struct dir_list_entry> dir_list;


	struct Inode
	{
		unsigned int block;
		bool type;
		unsigned int size;
		int addrs[NUM_OF_ADDRESS];
	};
	

	dir_list fileList = dir_list();
	/**
	 * format method
	 * This function discards the current content in the blockdevice and
	 * create a fresh new MYFS instance in the blockdevi	template <typename T>
	void read(int addr, int size, T *ans);
	template <typename T>
	void write(int addr, int size, T* ans);ce.
	 */
	void format();

	/**
	 * create_file method
	 * Creates a new file in the required path.
	 * @param path_str the file path (e.g. "/newfile")
	 * @param directory boolean indicating whether this is a file or directory
	 */
	void create_file(std::string path_str, bool directory);

	/**
	 * get_content method
	 * Returns the whole content of the file indicated by path_str param.
	 * Note: this method assumes path_str refers to a file and not a
	 * directory.
	 * @param path_str the file path (e.g. "/somefile")
	 * @return the content of the file
	 */
	std::string get_content(std::string path_str);

	/**
	 * set_content method
	 * Sets the whole content of the file indicated by path_str param.
	 * Note: this method assumes path_str refers to a file and not a
	 * directory.
	 * @param path_str the file path (e.g. "/somefile")
	 * @param content the file content string
	 */
	void set_content(std::string path_str, std::string content);

	/**
	 * list_dir method
	 * Returns a list of a files in a directory.
	 * Note: this method assumes path_str refers to a directory and not a
	 * file.
	 * @param path_str the file path (e.g. "/somedir")
	 * @return a vector of dir_list_entry structures, one for each file in
	 *	the directory.
	 */
	dir_list list_dir(std::string path_str);

private:

	/**
	 * This struct represents the first bytes of a myfs filesystem.
	 * It holds some magic characters and a number indicating the version.
	 * Upon class construction, the magic and the header are tested - if
	 * they both exist than the file is assumed to contain a valid myfs
	 * instance. Otherwise, the blockdevice is formated and a new instance is
	 * created.
	 */
	struct myfs_header {
		char magic[4];
		uint8_t version;
	};

	BlockDeviceSimulator *blkdevsim;

	static const uint8_t CURR_VERSION = 0x03;
	static const char *MYFS_MAGIC;

	int _nextFreeAddr;
	int _inodeCount;
	};

#endif // __MYFS_H__
