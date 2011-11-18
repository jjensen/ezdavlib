#include "webdav.h"
#include "high_level_webdav_functions.h"

int main() {
	// Initialize Winsock.
	WSADATA wsaData;
	int iResult = WSAStartup( MAKEWORD(2,2), &wsaData );

	HTTP_CONNECTION* connection;
	DAV_OPENDIR_DATA oddata;

//	int ret = http_connect(&connection, "demo.sabredav.org", 80, "testuser", "test");
//	int ret = dav_connect(&connection, "demo.sabredav.org", 80, "testuser", "test");
	int ret = dav_connect(&connection, "localhost", 8080, NULL, NULL);

//	ret = dav_copy_to_server(connection, "d:/inffas32.obj", "/inffas32.obj");
//	ret = dav_delete(connection, "/inffas32.obj");

	ret = dav_opendir(connection, "/", &oddata);
	while (ret) {
		int hi = 5;
		ret = dav_readdir(&oddata);
		if (!ret)
			break;
		hi = 10;
	}
	dav_closedir(&oddata);

//	ret = dav_copy_from_server(connection, "/public/Modern Photo Letter.pdf", "a.pdf");
	ret = dav_copy_from_server(connection, "/document.pdf", "u:/d.pdf");
//	ret = dav_copy_to_server(connection, "u:/screen2.jpg", "/screen2.jpg");
	ret = dav_copy_to_server(connection, "u:/Quicken_Deluxe_2007.exe", "/qd.exe");

	dav_disconnect(&connection);

	return 0;
}
