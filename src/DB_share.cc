#include "DB_share.h"

DB_share::DB_share()
: DB_Hash(DB)
{
	DB.query("CREATE TABLE IF NOT EXISTS share (hash TEXT, size TEXT, path TEXT)");
	DB.query("CREATE INDEX IF NOT EXISTS share_hash_index ON share (hash)");
	DB.query("CREATE INDEX IF NOT EXISTS share_path_index ON share (path)");
}

//std::pair<true if found, key>
static int add_entry_call_back(std::pair<bool,std::string> & info, int columns_retrieved,
	char ** response, char ** column_name)
{
	assert(response[0]);
	info.first = true;
	info.second = response[0];
	return 0;
}

void DB_share::add_entry(const std::string & hash, const boost::uint64_t & size, const std::string & path)
{
	//entry doesn't yet exist
	char * path_sqlite = sqlite3_mprintf("%q", path.c_str());
	std::ostringstream ss;
	ss << "INSERT INTO share(hash, size, path) VALUES('" << hash << "', '" << size << "', '" << path_sqlite << "')";
	sqlite3_free(path_sqlite);
	DB.query(ss.str());
}

void DB_share::delete_entry(const std::string & hash, const std::string & path)
{
	char * sqlite3_path = sqlite3_mprintf("%Q", path.c_str());
	DB.query("BEGIN TRANSACTION");
	boost::uint64_t file_size;
	if(lookup_hash(hash, file_size)){
		DB_Hash.delete_tree(hash, hash_tree::file_size_to_tree_size(file_size));
	}
	std::stringstream ss;
	ss << "DELETE FROM share WHERE hash = '" << hash << "' AND path = " << sqlite3_path;
	sqlite3_free(sqlite3_path);
	DB.query(ss.str());
	DB.query("END TRANSACTION");
}

static int lookup_hash_0_call_back(bool & entry_exists, int columns_retrieved,
	char ** response, char ** column_name)
{
	assert(response[0]);
	entry_exists = strcmp(response[0], "0") != 0;
	return 0;
}

bool DB_share::lookup_hash(const std::string & hash)
{
	bool entry_exists = false;
	std::ostringstream query;
	query << "SELECT count(1) FROM share WHERE hash = '" << hash << "' LIMIT 1";
	DB.query(query.str(), &lookup_hash_0_call_back, entry_exists);
	return entry_exists;
}

//std::pair<true if found, file path>
static int lookup_hash_1_call_back(std::pair<bool, std::string *> & info,
	int columns_retrieved, char ** response, char ** column_name)
{
	assert(response[0]);
	info.first = true;
	*info.second = response[0];
	return 0;
}

bool DB_share::lookup_hash(const std::string & hash, std::string & path)
{
	std::pair<bool, std::string *> info(false, &path);
	std::ostringstream query;
	query << "SELECT path FROM share WHERE hash = '" << hash << "' LIMIT 1";
	DB.query(query.str(), &lookup_hash_1_call_back, info);
	return info.first;
}

//std::pair<true if found, file_size>
static int lookup_hash_2_call_back(std::pair<bool, boost::uint64_t *> & info,
	int columns_retrieved, char ** response, char ** column_name)
{
	info.first = true;
	std::stringstream ss(response[0]);
	ss >> *info.second;
	return 0;
}

bool DB_share::lookup_hash(const std::string & hash, boost::uint64_t & file_size)
{
	std::pair<bool, boost::uint64_t *> info(false, &file_size);
	std::ostringstream query;
	query << "SELECT size FROM share WHERE hash = '" << hash << "' LIMIT 1";
	DB.query(query.str(), &lookup_hash_2_call_back, info);
	return info.first;
}

static int lookup_hash_3_call_back(boost::tuple<bool, boost::uint64_t *, std::string *> & info,
	int columns_retrieved, char ** response, char ** column_name)
{
	assert(response[0] && response[1]);
	info.get<0>() = true;
	std::stringstream ss(response[0]);
	ss >> *info.get<1>();
	*info.get<2>() = response[1];
	return 0;
}

bool DB_share::lookup_hash(const std::string & hash, std::string & path, boost::uint64_t & file_size)
{
	boost::tuple<bool, boost::uint64_t *, std::string *> info(false, &file_size, &path);
	std::ostringstream query;
	query << "SELECT size, path FROM share WHERE hash = '" << hash << "' LIMIT 1";
	DB.query(query.str(), &lookup_hash_3_call_back, info);
	return info.get<0>();
}

static int lookup_path_call_back(boost::tuple<bool, std::string *, boost::uint64_t *> & info,
	int columns_retrieved, char ** response, char ** column_name)
{
	assert(response[0] && response[1]);
	info.get<0>() = true;
	*info.get<1>() = response[0];
	std::stringstream ss;
	ss << response[1];
	ss >> *info.get<2>();
	return 0;
}

bool DB_share::lookup_path(const std::string & path, std::string & hash, boost::uint64_t & size)
{
	boost::tuple<bool, std::string *, boost::uint64_t *> info(false, &hash, &size);
	char * query = sqlite3_mprintf("SELECT hash, size FROM share WHERE path = '%q' LIMIT 1", path.c_str());
	DB.query(query, &lookup_path_call_back, info);
	sqlite3_free(query);
	return info.get<0>();
}

int DB_share::remove_missing_call_back(std::map<std::string, std::string> & missing,
	int columns_retrieved, char ** response, char ** column_name)
{
//DEBUG, very bad for performance to do file access in here
//this will be placed with path_memoize eventually
	assert(response[0] && response[1]);
	std::fstream fin(response[1], std::ios::in);
	if(!fin.is_open()){
		missing.insert(std::make_pair(response[0], response[1]));
	}
	return 0;
}

void DB_share::remove_missing(const std::string & share_directory)
{
	std::map<std::string, std::string> missing;
	DB.query("SELECT hash, path FROM share", this, &DB_share::remove_missing_call_back, missing);
	std::map<std::string, std::string>::iterator iter_cur, iter_end;
	iter_cur = missing.begin();
	iter_end = missing.end();
	while(iter_cur != iter_end){
		delete_entry(iter_cur->first, iter_cur->second);
		++iter_cur;
	}
}

static int unique_key_call_back(bool & unique, int columns_retrieved,
	char ** response, char ** column_name)
{
	assert(response[0]);
	unique = strcmp(response[0], "1") == 0;
	return 0;
}

bool DB_share::unique_hash(const std::string & hash)
{
	bool unique = false;
	std::stringstream query;
	query << "SELECT count(1) FROM share WHERE hash = '" << hash << "'";
	DB.query(query.str(), &unique_key_call_back, unique);
	return unique;
}
