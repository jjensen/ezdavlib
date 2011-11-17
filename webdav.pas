unit webdav;

interface

type
  HTTP_CONNECTION = Pointer;

  DAV_OPENDIR_DATA = record
    href : PChar;
    prop : Pointer;
    filename : PChar;
    lockowner : PChar;
    filetype : integer;
    size : integer;
    cdate : integer;
    mdate : integer;
    multistatus : Pointer;
    response_cursor : Pointer;
    directory_length : integer;
    additional_prop : PChar;
  end;

function dav_initialize_lock_database : boolean; cdecl;
procedure dav_finalize_lock_database; cdecl;
function dav_save_lock_database(filepath : PChar) : boolean; cdecl;
function dav_load_lock_database(filepath : PChar) : boolean; cdecl;
function dav_connect(var Connection : HTTP_CONNECTION;
                     Host : PChar;
                     Port : Smallint;
                     User : PChar;
                     Password : PChar) : boolean; cdecl;
function dav_disconnect(var Connection : HTTP_CONNECTION) : boolean; cdecl;
function dav_opendir(connection : HTTP_CONNECTION;
                     directory : PChar;
                     var oddata : DAV_OPENDIR_DATA) : boolean; cdecl;
function dav_opendir_ex(connection : HTTP_CONNECTION;
                        directory : PChar;
                        additional_prop : PChar;
                        var oddata : DAV_OPENDIR_DATA) : boolean; cdecl;
function dav_readdir(var oddata : DAV_OPENDIR_DATA) : boolean; cdecl;
function dav_closedir(var oddata : DAV_OPENDIR_DATA) : boolean; cdecl;
function dav_mkdir(connection : HTTP_CONNECTION; dir : PChar) : boolean; cdecl;
function dav_delete(connection : HTTP_CONNECTION; resource : PChar) : boolean; cdecl;
function dav_copy_to_server(connection : HTTP_CONNECTION; src, dest : PChar) : boolean; cdecl;
function dav_copy_from_server(connection : HTTP_CONNECTION; src, dest : PChar) : boolean; cdecl;
function dav_lock(connection : HTTP_CONNECTION; resource, owner : PChar) : boolean; cdecl;
function dav_unlock(connection : HTTP_CONNECTION; resource : PChar) : boolean; cdecl;

implementation

function dav_initialize_lock_database : boolean; cdecl;
external 'webdav.dll' name 'dav_initialize_lock_database';

procedure dav_finalize_lock_database; cdecl;
external 'webdav.dll' name 'dav_finalize_lock_database';

function dav_save_lock_database(filepath : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_save_lock_database';

function dav_load_lock_database(filepath : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_load_lock_database';

function dav_connect(var Connection : HTTP_CONNECTION;
                     Host : PChar;
                     Port : Smallint;
                     User : PChar;
                     Password : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_connect';

function dav_disconnect(var Connection : HTTP_CONNECTION) : boolean; cdecl;
external 'webdav.dll' name 'dav_disconnect';

function dav_opendir(connection : HTTP_CONNECTION;
                     directory : PChar;
                     var oddata : DAV_OPENDIR_DATA) : boolean; cdecl;
external 'webdav.dll' name 'dav_opendir';

function dav_opendir_ex(connection : HTTP_CONNECTION;
                        directory : PChar;
                        additional_prop : PChar;
                        var oddata : DAV_OPENDIR_DATA) : boolean; cdecl;
external 'webdav.dll' name 'dav_opendir_ex';

function dav_readdir(var oddata : DAV_OPENDIR_DATA) : boolean; cdecl;
external 'webdav.dll' name 'dav_readdir';

function dav_closedir(var oddata : DAV_OPENDIR_DATA) : boolean; cdecl;
external 'webdav.dll' name 'dav_closedir';

function dav_mkdir(connection : HTTP_CONNECTION; dir : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_mkdir';

function dav_delete(connection : HTTP_CONNECTION; resource : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_delete';

function dav_copy_to_server(connection : HTTP_CONNECTION; src, dest : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_copy_to_server';

function dav_copy_from_server(connection : HTTP_CONNECTION; src, dest : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_copy_from_server';

function dav_lock(connection : HTTP_CONNECTION; resource, owner : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_lock';

function dav_unlock(connection : HTTP_CONNECTION; resource : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_unlock';

function dav_abandon_lock(connection : HTTP_CONNECTION; resource : PChar) : boolean; cdecl;
external 'webdav.dll' name 'dav_abandon_lock';

function dav_exec_error : integer; cdecl;
external 'webdav.dll' name 'dav_exec_error';

function dav_exec_error_msg : PChar; cdecl;
external 'webdav.dll' name 'dav_exec_error_msg';

end.
