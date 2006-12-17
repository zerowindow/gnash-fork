// 
//   Copyright (C) 2005, 2006 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#include <cstdarg>

#include <mysql/errmsg.h>
#include <mysql/mysql.h>
#include <iostream>
#include <vector>

#include "log.h"
#include "array.h"
#include "as_value.h"
#include "fn_call.h"
#include "mysql_db.h"
#include "builtin_function.h" // need builtin_function

using namespace std;

namespace gnash
{

void mysql_connect(const fn_call& fn);
void mysql_qetData(const fn_call& fn);
void mysql_disconnect(const fn_call& fn);

void mysql_query(const fn_call& fn);
void mysql_row(const fn_call& fn);
void mysql_fields(const fn_call& fn);
void mysql_fetch(const fn_call& fn);
void mysql_store(const fn_call& fn);
void mysql_free(const fn_call& fn);

LogFile& dbglogfile = LogFile::getDefaultInstance();

class mysql_as_object : public as_object
{
public:
    MySQL obj;
};

static void
attachInterface(as_object *obj)
{
//    GNASH_REPORT_FUNCTION;

    obj->set_member("connect", &mysql_connect);
    obj->set_member("qetData", &mysql_qetData);
    obj->set_member("disconnect", &mysql_disconnect);
    obj->set_member("query", &mysql_query);
    obj->set_member("fetch_row", &mysql_fetch);
    obj->set_member("num_fields", &mysql_fields);
    obj->set_member("free_result", &mysql_free);
    obj->set_member("store_results", &mysql_store);
}

static as_object*
getInterface()
{
//    GNASH_REPORT_FUNCTION;

    static boost::intrusive_ptr<as_object> o;
    if (o == NULL) {
	o = new as_object();
    }
    return o.get();
}

static void
mysql_ctor(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;

    mysql_as_object* obj = new mysql_as_object();

    attachInterface(obj);
    fn.result->set_as_object(obj); // will keep alive
//    printf ("Hello World from %s !!!\n", __PRETTY_FUNCTION__);
}


MySQL::MySQL(): _db(NULL), _result(NULL), _row(NULL)
{
//    GNASH_REPORT_FUNCTION;
}

MySQL::~MySQL()
{
//    GNASH_REPORT_FUNCTION;
    disconnect();
}

int
MySQL::num_fields()
{
//    GNASH_REPORT_FUNCTION;
    if (_result) {
	return num_fields(_result);
    }
    return -1;
}

int
MySQL::num_fields(MYSQL_RES *result)
{
//    GNASH_REPORT_FUNCTION;
    return mysql_num_fields(result);
}

MYSQL_ROW
MySQL::fetch_row()
{
//    GNASH_REPORT_FUNCTION;
    if (_result) {
	return fetch_row(_result);
    }
    return NULL;
}

MYSQL_ROW
MySQL::fetch_row(MYSQL_RES *result)
{
//    GNASH_REPORT_FUNCTION;
    return mysql_fetch_row(result);
}

void
MySQL::free_result()
{
//    GNASH_REPORT_FUNCTION;
    if (_result) {
	free_result(_result);
    }
}

void
MySQL::free_result(MYSQL_RES *result)
{
//    GNASH_REPORT_FUNCTION;
    mysql_free_result(result);
}

MYSQL_RES *
MySQL::store_result()
{
//    GNASH_REPORT_FUNCTION;
    if (_db) {
	return store_result(_db);
    }
    return NULL;
}

MYSQL_RES *
MySQL::store_result(MYSQL *db)
{
//    GNASH_REPORT_FUNCTION;
    _result = mysql_store_result(db);
    return _result;
}

bool
MySQL::connect(const char* host, const char* dbname, const char* user, const char* passwd)
{
//    GNASH_REPORT_FUNCTION;

    // Closes a previously opened connection &
    // also deallocates the connection handle
    disconnect();
    
    if ((_db = mysql_init(NULL)) == NULL ) {
	log_error("Couldn't initialize Database!");
	return false;
    }
    
    if (mysql_real_connect(_db, host, user, passwd, dbname, 0, NULL, 0) == NULL) {
	log_error("Couldn't connect to Database!");
	return false;
    }
    
    return true;
}

bool
MySQL::guery(const char *sql)
{
//    GNASH_REPORT_FUNCTION;
    if (_db) {
	return guery(_db, sql);
    }
    return -1;
}

bool
MySQL::guery(MYSQL *db, const char *sql)
{
//    GNASH_REPORT_FUNCTION;
    int res = mysql_real_query(db, sql, strlen(sql));
    switch (res) {
      case CR_SERVER_LOST:
      case CR_COMMANDS_OUT_OF_SYNC:
      case CR_SERVER_GONE_ERROR:
	  dbglogfile << "MySQL connection error: " << mysql_error(_db) << endl;
	  // Try to reconnect to the database
// 	  closeDB();
// 	  openDB();
	  break;
      case -1:
      case CR_UNKNOWN_ERROR:
	  dbglogfile << "MySQL error on query for:\n\t " <<
	      mysql_error(_db) << endl;
	  dbglogfile << "Query was: " << sql << endl;
	  return false;
	  break;            
       default:
 	  return true;
    } 
}

int
MySQL::getData(const char *sql, query_t &qresult)
{
//    GNASH_REPORT_FUNCTION;

    bool qstatus = false;
    int res = mysql_real_query(_db, sql, strlen(sql));
    switch (res) {
      case CR_SERVER_LOST:
      case CR_COMMANDS_OUT_OF_SYNC:
      case CR_SERVER_GONE_ERROR:
	  dbglogfile << "MySQL connection error: " << mysql_error(_db) << endl;
	  // Try to reconnect to the database
// 	  closeDB();
// 	  openDB();
	  break;
      case -1:
      case CR_UNKNOWN_ERROR:
	  dbglogfile << "MySQL error on query for:\n\t " <<
	      mysql_error(_db) << endl;
	  dbglogfile << "Query was: " << sql << endl;
//	  return false;
	  break;            
//       default:
// 	  return true;
    }    

    _result = mysql_store_result(_db);
//    int nrows = mysql_num_rows(result);

#if 0
    for (size_t i=0; i<mysql_num_fields(_result); i++) {
	MYSQL_FIELD *fields = mysql_fetch_fields(_result);
	log_msg("Field name is: %s: ", fields->name);
    }
#endif
    
    while((_row = mysql_fetch_row(_result))) {
	vector<const char *> row_vec;
	for (size_t i=0; i<mysql_num_fields(_result); i++) {
//	    log_msg("Column[%d] is: \"%s\"", i, row[i]);
	    row_vec.push_back(_row[i]);
        }
	qresult.push_back(row_vec);
	qstatus = true;
    }

    mysql_free_result(_result);
    return(qstatus);
}

bool
MySQL::disconnect()
{
//    GNASH_REPORT_FUNCTION;
    if (_db != NULL) {
        mysql_close(_db);    
        _db = NULL;
    }
    return true;
}


// Entry points for ActionScript methods
void
mysql_connect(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;

    mysql_as_object *ptr = (mysql_as_object*)fn.this_ptr;
    assert(ptr);

    if (fn.nargs == 4) {
	const char *host = fn.env->bottom(fn.first_arg_bottom_index).to_string();
	const char *db = fn.env->bottom(fn.first_arg_bottom_index-1).to_string();
	const char *user = fn.env->bottom(fn.first_arg_bottom_index-2).to_string();
	const char *passwd = fn.env->bottom(fn.first_arg_bottom_index-3).to_string();	
	fn.result->set_bool(ptr->obj.connect(host, db, user, passwd));
    } else {
	fn.result->set_bool(false);
    }
}

void
mysql_qetData(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;

    mysql_as_object *ptr = (mysql_as_object*)fn.this_ptr;
    assert(ptr);

    if (fn.nargs > 0) {
	const char *sql = fn.env->bottom(fn.first_arg_bottom_index).to_string();
	as_array_object *arr = (as_array_object *)fn.env->bottom(fn.first_arg_bottom_index-1).to_object();
//	std::vector< std::vector<const char *> >
	MySQL::query_t qresult;
	fn.result->set_bool(ptr->obj.getData(sql, qresult));
	for (size_t i=0; i<qresult.size(); i++) {
	    vector<const char *> row;
	    row = qresult[i];
	    for (size_t j=0; j< row.size(); j++) {
//		cerr << "ARR: " << i << ":" << j << " " << row[j] << endl;
		as_value entry = row[j];
		arr->push(entry);
	    }
	}
 	fn.result->set_bool(true);
//     } else {
// 	fn.result->set_bool(false);
    }
}

void
mysql_free(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;
    mysql_as_object *ptr = (mysql_as_object*)fn.this_ptr;
    assert(ptr);
    ptr->obj.free_result();
    fn.result->set_bool(true);
}

void
mysql_fields(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;
    mysql_as_object *ptr = (mysql_as_object*)fn.this_ptr;
    assert(ptr);
    fn.result->set_int(ptr->obj.num_fields());
}

void
mysql_fetch(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;
    if (fn.nargs > 0) {
	mysql_as_object *ptr = (mysql_as_object*)fn.this_ptr;
	assert(ptr);
	MYSQL_ROW res = ptr->obj.fetch_row();
	as_value aaa = *res;       
	as_array_object *arr = new as_array_object;
	arr->push(aaa);
	fn.result->set_as_object(arr);
    }
}

void
mysql_store(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;
    mysql_as_object *ptr = (mysql_as_object*)fn.this_ptr;
    assert(ptr);
    fn.result->set_bool(ptr->obj.store_result());
}

void
mysql_query(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;
    mysql_as_object *ptr = (mysql_as_object*)fn.this_ptr;
    if (fn.nargs > 0) {
	const char *sql = fn.env->bottom(fn.first_arg_bottom_index).to_string();
	assert(ptr);
	fn.result->set_int(ptr->obj.guery(sql));
    }
}

void
mysql_disconnect(const fn_call& fn)
{
//    GNASH_REPORT_FUNCTION;

    mysql_as_object *ptr = (mysql_as_object*)fn.this_ptr;
    assert(ptr);
    fn.result->set_bool(ptr->obj.disconnect());
}

extern "C" {
    void
    mysql_class_init(as_object &obj)
    {
//	GNASH_REPORT_FUNCTION;
	// This is going to be the global "class"/"function"
	static boost::intrusive_ptr<builtin_function> cl;
	if (cl == NULL) {
	    cl = new builtin_function(&mysql_ctor, getInterface());
// 	    // replicate all interface to class, to be able to access
// 	    // all methods as static functions
 	    attachInterface(cl.get());
	}	
	obj.set_member("MySQL", cl.get());
    }
    
} // end of extern C

} // end of gnash namespace


// Local Variables:
// mode: C++
// indent-tabs-mode: t
// End:
