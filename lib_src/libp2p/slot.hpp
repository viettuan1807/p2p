#ifndef H_SLOT
#define H_SLOT

//custom
#include "database.hpp"
#include "file.hpp"
#include "file_info.hpp"
#include "hash_tree.hpp"

//include
#include <atomic_bool.hpp>
#include <boost/cstdint.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>

//standard
#include <string>
#include <vector>

class share;
class slot : private boost::noncopyable
{
	/*
	Slots are unique (there exists only one slot per hash). Only the share class
	is able to instantiate a slot. This makes it easy to centrally enforce this.
	*/
	friend class share;
public:

	/* Slot Info Functions
	complete:
		Returns true if both the Hash_Tree and File are complete.
	download_speed:
		Returns total download speed (bytes/second).
	file_size:
		Returns size of the file the slot is for.
	hash:
		Returns root hash of the hash tree (hex).
	merge_host:
		Inserts all hosts known to have this file in to host_in.
	name:
		Returns the name of the file.
	percent_complete:
		Returns percent complete (0-100).
	tree_size:
		Returns the size of the hash tree for the file.
	upload_speed:
		Returns total upload speed (bytes/second).
	*/
	bool complete();
	unsigned download_speed();
	const boost::uint64_t & file_size();
	const std::string & hash();
	void merge_host(std::set<database::table::host::host_info> & host_in);
	std::string name();
	unsigned percent_complete();
	const boost::uint64_t & tree_size();
	unsigned upload_speed();

private:
	//the ctor will throw an exception if database access fails
	slot(
		const file_info & FI,
		database::pool::proxy DB = database::pool::proxy()
	);

	//all access to Hash_Tree must be locked with Hash_Tree_mutex
	hash_tree Hash_Tree;

	//all access to File must be locked with File_mutex
	file File;

	//all the hosts known to have the file
	boost::mutex host_mutex;
	std::set<database::table::host::host_info> host;
};
#endif
