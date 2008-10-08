#include "DB_share.h"

DB_share::DB_share()
{
	//open DB
	if(sqlite3_open(global::DATABASE_PATH.c_str(), &sqlite3_DB) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}

	//DB timeout to 1 second
	if(sqlite3_busy_timeout(sqlite3_DB, 1000) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}

	if(sqlite3_exec(sqlite3_DB, "CREATE TABLE IF NOT EXISTS share (hash TEXT, size TEXT, path TEXT)", NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	if(sqlite3_exec(sqlite3_DB, "CREATE INDEX IF NOT EXISTS share_path_index ON share (path)", NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	if(sqlite3_exec(sqlite3_DB, "CREATE INDEX IF NOT EXISTS share_hash_index ON share (hash)", NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
}

void DB_share::add_entry(const std::string & hash, const boost::uint64_t & size, const std::string & path)
{
	boost::mutex::scoped_lock lock(Mutex);

	//if this is set to true after the query the entry exists in the database
	add_entry_entry_exists = false;

	//determine if the entry already exists
	char * path_sqlite = sqlite3_mprintf("%q", path.c_str());
	std::ostringstream query;
	query << "SELECT * FROM share WHERE path = '" << path_sqlite << "' LIMIT 1";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), add_entry_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	sqlite3_free(path_sqlite);

	if(!add_entry_entry_exists){
		char * path_sqlite = sqlite3_mprintf("%q", path.c_str());
		std::ostringstream query;
		query << "INSERT INTO share (hash, size, path) VALUES ('" << hash << "', '" << size << "', '" << path_sqlite << "')";
		if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
			logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
		}
		sqlite3_free(path_sqlite);
	}
}

void DB_share::add_entry_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	add_entry_entry_exists = true;
}

void DB_share::delete_hash(const std::string & hash)
{
	std::ostringstream query;
	query << "DELETE FROM share WHERE hash = '" << hash << "'";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
}

bool DB_share::hash_exists(const std::string & hash)
{
	std::ostringstream query;
	query << "SELECT count(1) FROM share WHERE hash = '" << hash << "'";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), hash_exists_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	return hash_exists_exists;
}

void DB_share::hash_exists_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	hash_exists_exists = strcmp(query_response[0], "1") == 0;
}

bool DB_share::lookup_path(const std::string & path, std::string & hash, boost::uint64_t & size)
{
	boost::mutex::scoped_lock lock(Mutex);
	lookup_path_entry_exists = false;
	lookup_path_hash = &hash;
	lookup_path_size = &size;

	char * query = sqlite3_mprintf("SELECT hash, size FROM share WHERE path = '%q' LIMIT 1", path.c_str());
	if(sqlite3_exec(sqlite3_DB, query, lookup_path_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	sqlite3_free(query);
	return lookup_path_entry_exists;
}

void DB_share::lookup_path_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	lookup_path_entry_exists = true;
	*lookup_path_hash = query_response[0];

	std::istringstream size_iss(query_response[1]);
	size_iss >> *lookup_path_size;
}

bool DB_share::lookup_hash(const std::string & hash, std::string & path)
{
	boost::mutex::scoped_lock lock(Mutex);
	lookup_hash_entry_exists = false;
	lookup_hash_path = &path;

	std::ostringstream query;
	query << "SELECT path FROM share WHERE hash = '" << hash << "'";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), lookup_hash_1_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	return lookup_hash_entry_exists;
}

bool DB_share::lookup_hash(const std::string & hash, std::string & path, boost::uint64_t & size)
{
	boost::mutex::scoped_lock lock(Mutex);
	lookup_hash_entry_exists = false;
	lookup_hash_path = &path;
	lookup_hash_size = &size;

	std::ostringstream query;
	query << "SELECT size, path FROM share WHERE hash = '" << hash << "'";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), lookup_hash_2_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	return lookup_hash_entry_exists;
}

void DB_share::lookup_hash_1_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	lookup_hash_entry_exists = true;
	*lookup_hash_path = query_response[0];
}

void DB_share::lookup_hash_2_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	lookup_hash_entry_exists = true;

	std::istringstream size_iss(query_response[0]);
	size_iss >> *lookup_hash_size;

	*lookup_hash_path = query_response[1];
}

void DB_share::remove_missing(const std::string & share_directory)
{
	if(sqlite3_exec(sqlite3_DB, "SELECT hash, path FROM share", remove_missing_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
}

void DB_share::remove_missing_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(Mutex);
	std::fstream fin(query_response[1], std::ios::in);
	if(!fin.is_open()){
		std::string orphan_hash = global::HASH_DIRECTORY+std::string(query_response[0]);
		std::remove(orphan_hash.c_str());
		std::ostringstream query;
		query << "DELETE FROM share WHERE hash = '" << query_response[0] << "'";
		if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
			logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
		}
	}
	}//end lock scope
}