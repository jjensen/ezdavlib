#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "http.h"
#include "strutl.h"
#include "global.h"
#include "digcalc.h"
#include "date_decode.h"

const char *http_method[9] = { "GET", "PUT", "POST", "LOCK", "UNLOCK", "PROPFIND", "PROPPATCH", "MKCOL", "DELETE" };

void
http_append_auth_request_parameter(HTTP_AUTH_INFO *info, HTTP_AUTH_PARAMETER *parameter)
{
if(info->first_parameter == NULL)
 {
 info->first_parameter = parameter;
 }
else
 {
 info->last_parameter->next_parameter = parameter;
 }
parameter->prev_parameter = info->last_parameter;
info->last_parameter = parameter;
}

int
http_add_auth_parameter(HTTP_AUTH_INFO *info, const char *name, const char *value)
{
HTTP_AUTH_PARAMETER *new_parameter = NULL;
if(info == NULL || name == NULL || value == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
new_parameter = (HTTP_AUTH_PARAMETER *) malloc(sizeof(HTTP_AUTH_PARAMETER));
if(new_parameter == NULL)
 {
 return HT_MEMORY_ERROR;
 }
memset(new_parameter, 0, sizeof(HTTP_AUTH_PARAMETER));
new_parameter->name = strdup(name);
new_parameter->value = strdup(value);
if(new_parameter->name == NULL || new_parameter->value == NULL)
 {
 return HT_MEMORY_ERROR;
 }
http_append_auth_request_parameter(info, new_parameter);
return HT_OK;
}

int
http_connect(HTTP_CONNECTION **connection, const char *host, short port, const char *username, const char *password)
{
unsigned int ipaddr = 0;
struct hostent *hostinfo = NULL;
HTTP_CONNECTION *new_connection = NULL;
if(connection == NULL || host == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
new_connection = (HTTP_CONNECTION *) malloc(sizeof(HTTP_CONNECTION));
memset(new_connection, 0, sizeof(HTTP_CONNECTION));
new_connection->address.sin_family = PF_INET;
new_connection->address.sin_port = (port << 8) | (port >> 8);	/* convert to big edian */
if(username != NULL && password != NULL)
 {
 new_connection->auth_info = (HTTP_AUTH_INFO *) malloc(sizeof(HTTP_AUTH_INFO));
 if(new_connection->auth_info == NULL)
  {
  http_disconnect(&new_connection);
  return HT_MEMORY_ERROR;
  }
 memset(new_connection->auth_info, 0, sizeof(HTTP_AUTH_INFO));
 http_add_auth_parameter(new_connection->auth_info, "username", username);
 http_add_auth_parameter(new_connection->auth_info, "password", password);
 }
if((ipaddr = inet_addr(host)) != INADDR_NONE)
 {
 memcpy(&new_connection->address.sin_addr, &ipaddr, sizeof(struct in_addr));
 }
else
 {
 hostinfo = (struct hostent *) gethostbyname(host);
 if(hostinfo == NULL)
  {
  return HT_HOST_UNAVAILABLE;
  }
 memcpy(&new_connection->address.sin_addr, hostinfo->h_addr, 4);
 new_connection->host = strdup(host);
 if(new_connection->host == NULL)
  {
  http_disconnect(&new_connection);
  return HT_MEMORY_ERROR;
  }
 }
new_connection->socketd = socket(PF_INET, SOCK_STREAM, 0);
if(new_connection->socketd == INVALID_SOCKET)
 {
 http_disconnect(&new_connection);
 return HT_RESOURCE_UNAVAILABLE;
 }
if(connect(new_connection->socketd, (struct sockaddr *) &new_connection->address, sizeof(struct sockaddr_in)) != 0)
 {
 http_disconnect(&new_connection);
 return HT_NETWORK_ERROR;
 }
new_connection->persistent = TRUE;
new_connection->status = HT_OK;
*connection = new_connection;
return HT_OK;
}

int
http_check_socket(HTTP_CONNECTION *connection)
{
FD_SET socket_set;
TIMEVAL timeval = { 0, 0 };
FD_ZERO(&socket_set);
FD_SET(connection->socketd, &socket_set);
if(select(0, &socket_set, NULL, NULL, &timeval) == 1)
 {
 return HT_NETWORK_ERROR;
 }
return HT_OK;
}

int
http_reconnect(HTTP_CONNECTION *connection)
{
if(connection == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
if(connection->socketd != INVALID_SOCKET && connection->status == HT_OK)
 {
 if(http_check_socket(connection) == HT_OK)
  {
  return HT_OK;
  }
 }
if(!connection->persistent)
 {
 return HT_ILLEGAL_OPERATION;
 }
if(connection->socketd != INVALID_SOCKET)
 {
 close(connection->socketd);
 }
connection->socketd = socket(PF_INET, SOCK_STREAM, 0);
if(connection->socketd == INVALID_SOCKET)
 {
 return HT_RESOURCE_UNAVAILABLE;
 }
if(connect(connection->socketd, (struct sockaddr *) &connection->address, sizeof(struct sockaddr_in)) != 0)
 {
 close(connection->socketd);
 return HT_NETWORK_ERROR;
 }
connection->status = HT_OK;
return HT_OK;
}

int
http_request_reconnection(HTTP_CONNECTION *connection)
{
if(!connection->persistent)
 {
 return HT_ILLEGAL_OPERATION;
 }
close(connection->socketd);
connection->socketd = INVALID_SOCKET;
return HT_OK;
}

void http_destroy_auth_parameter(HTTP_AUTH_PARAMETER **parameter)
{
if(parameter != NULL && *parameter != NULL)
 {
 free((*parameter)->name);
 free((*parameter)->value);
 free(*parameter);
 }
}
  
void http_destroy_auth_info(HTTP_AUTH_INFO **auth_info)
{
HTTP_AUTH_PARAMETER *parameter_cursor = NULL, *next_parameter = NULL;
if(auth_info != NULL && *auth_info != NULL)
 {
 for(parameter_cursor = (*auth_info)->first_parameter; parameter_cursor != NULL; parameter_cursor = next_parameter)
  {
  next_parameter = parameter_cursor->next_parameter;
  http_destroy_auth_parameter(&parameter_cursor);
  }
 free((*auth_info)->method);
 free(*auth_info);
 auth_info = NULL;
 }
}

int
http_disconnect(HTTP_CONNECTION **connection)
{
if(connection == NULL || *connection == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
close((*connection)->socketd);
free((*connection)->host);
http_destroy_auth_info(&(*connection)->auth_info);
free(*connection);
*connection = NULL;
return HT_OK;
}

int
http_send_strings(HTTP_CONNECTION *connection, const char *first_string, ...)
{
va_list marker;
const char *string = NULL;
int length, error = HT_OK;
if(connection->status == HT_OK)
 {
 va_start(marker, first_string);
 string = first_string;
 while(string != NULL)
  {
  if(string != NULL)
   {
   length = strlen(string);
   if(send(connection->socketd, string, length, 0) != length)
    {
    error = HT_NETWORK_ERROR;
	break;
    }
   }
  string = va_arg(marker, const char *);
  }  
 va_end(marker);
 connection->status = error;
 }
return connection->status;
}

int
http_send_storage(HTTP_CONNECTION *connection, HTTP_STORAGE *storage)
{
char read_buffer[128];
int read_count = 0, network_error = HT_OK, io_error;
if(connection->status == HT_OK)
 {
 http_storage_seek(storage, 0);
 while((io_error = http_storage_read(storage, read_buffer, 128, &read_count)) == HT_OK && read_count != 0 && network_error == HT_OK)
  {
  if(send(connection->socketd, read_buffer, read_count, 0) != read_count)
   {
   network_error = HT_NETWORK_ERROR;
   }
  }
 connection->status = network_error | io_error;
 }
return connection->status;
};

void http_destroy_header_field(HTTP_HEADER_FIELD **field)
{
if(field != NULL && *field != NULL)
 {
 free((*field)->name);
 free((*field)->value);
 free(*field);
 }
}
  
void http_destroy_request(HTTP_REQUEST **request)
{
HTTP_HEADER_FIELD *field_cursor = NULL, *next_field = NULL;
if(request != NULL && *request != NULL)
 {
 free((*request)->resource);
 for(field_cursor = (*request)->first_header_field; field_cursor != NULL; field_cursor = next_field)
  {
  next_field = field_cursor->next_field;
  http_destroy_header_field(&field_cursor);
  }
 http_storage_destroy(&(*request)->content);
 free(*request);
 *request = NULL;
 }
}

int
http_create_request(HTTP_REQUEST **request, int method, const char *resource)
{
HTTP_REQUEST *new_request = NULL;
if(request == NULL || resource == NULL || method < HTTP_FIRST_METHOD || method > HTTP_LAST_METHOD)
 {
 return HT_INVALID_ARGUMENT;
 }
new_request = (HTTP_REQUEST *) malloc(sizeof(HTTP_REQUEST));
if(new_request == NULL)
 {
 return HT_MEMORY_ERROR;
 }
memset(new_request, 0, sizeof(HTTP_REQUEST));
new_request->method = method;
new_request->resource = strdup_url_encoded(resource);
if(new_request->resource == NULL)
 {
 http_destroy_request(&new_request);
 return HT_MEMORY_ERROR;
 }
*request = new_request;
return HT_OK;
}

int
http_add_header_field(HTTP_REQUEST *request, const char *field_name, const char *field_value)
{
HTTP_HEADER_FIELD *new_header_field = NULL;
if(request == NULL || field_name == NULL || field_value == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
new_header_field = (HTTP_HEADER_FIELD *) malloc(sizeof(HTTP_HEADER_FIELD));
if(new_header_field == NULL)
 {
 return HT_MEMORY_ERROR;
 }
memset(new_header_field, 0, sizeof(HTTP_HEADER_FIELD));
new_header_field->name = strdup(field_name);
new_header_field->value = strdup(field_value);
if(new_header_field->name == NULL || new_header_field->value == NULL)
 {
 http_destroy_header_field(&new_header_field);
 return HT_MEMORY_ERROR;
 }
if(request->first_header_field == NULL)
 {
 request->first_header_field = new_header_field;
 }
else
 {
 request->last_header_field->next_field = new_header_field;
 }
new_header_field->prev_field = request->last_header_field;
request->last_header_field = new_header_field;
return HT_OK;
}

int
http_add_header_field_number(HTTP_REQUEST *request, const char *field_name, int field_value)
{
char number_buffer[32];
if(field_value == INFINITY)
 {
 return http_add_header_field(request, field_name, "infinity");
 }
else
 {
 sprintf(number_buffer, "%d", field_value);
 return http_add_header_field(request, field_name, number_buffer);
 }
}

const char *
http_find_auth_parameter(HTTP_AUTH_INFO *info, const char *parameter_name, const char *default_value)
{
HTTP_AUTH_PARAMETER *parameter_cursor = NULL;
if(info == NULL || parameter_name == NULL)
 {
 return default_value;
 }
for(parameter_cursor = info->first_parameter; parameter_cursor != NULL; parameter_cursor = parameter_cursor->next_parameter)
 {
 if(strcasecmp(parameter_cursor->name, parameter_name) == 0)
  {
  return parameter_cursor->value;
  }
 }
return default_value;
}

int
http_send_authorization_header_field(HTTP_CONNECTION *connection, HTTP_REQUEST *request)
{
char *credentials = NULL, *user_pass = NULL, nonce_count[9], cnonce[9];
const char *username = NULL, *password = NULL, *realm = NULL;
const char *nonce = NULL, *opaque = NULL, *algorithm = NULL;
const char *qop = NULL, *message_qop = "";
HASHHEX HA1;
HASHHEX HEntity = "";
HASHHEX response_digest;
if(connection == NULL || request == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
if(connection->auth_info == NULL || connection->auth_info->method == NULL)
 {
 return HT_SERVICE_UNAVAILABLE;
 }
if(strcasecmp(connection->auth_info->method, "basic") == 0)
 {
 connection->auth_info->count++;
 username = http_find_auth_parameter(connection->auth_info, "username", NULL);
 password = http_find_auth_parameter(connection->auth_info, "password", NULL);
 if(username == NULL || password == NULL)
  {
  return HT_RESOURCE_UNAVAILABLE;
  }
 user_pass = (char *) malloc((strlen(username) + 1 + strlen(password) + 1) * sizeof(char));
 strcpy(user_pass, username);
 strcat(user_pass, ":");
 strcat(user_pass, password);
 credentials = strdup_base64(user_pass);
 http_send_strings(connection, "Authorization: Basic ", credentials, "\r\n", NULL);
 free(credentials);
 free(user_pass);
 return HT_OK;
 }
else if(strcasecmp(connection->auth_info->method, "digest") == 0)
 {
 username = http_find_auth_parameter(connection->auth_info, "username", NULL);
 password = http_find_auth_parameter(connection->auth_info, "password", NULL);
 realm = http_find_auth_parameter(connection->auth_info, "realm", "");
 nonce = http_find_auth_parameter(connection->auth_info, "nonce", "");
 opaque = http_find_auth_parameter(connection->auth_info, "opaque", "");
 qop = http_find_auth_parameter(connection->auth_info, "qop", NULL);
 algorithm  = http_find_auth_parameter(connection->auth_info, "opaque", "MD5");
 if(username == NULL || password == NULL)
  {
  return HT_RESOURCE_UNAVAILABLE;
  }
 if(strcasecmp(algorithm, "MD5") != 0)
  {
  return HT_SERVICE_UNAVAILABLE;
  }
 if(qop != NULL)
  {
  message_qop = "auth";
  sprintf(nonce_count, "%08X", connection->auth_info->count);
  sprintf(cnonce, "%08x", time(NULL));
  }
 DigestCalcHA1(algorithm, username, realm, password, nonce, cnonce, HA1);
 DigestCalcResponse(HA1, nonce, nonce_count, cnonce, message_qop, http_method[request->method], request->resource, HEntity, response_digest);
 if(message_qop != NULL)
  {
  http_send_strings(connection, "Authorization: Digest username=\"", username, "\", realm=\"", realm, "\", nonce=\"", nonce, "\", uri=\"", request->resource, "\", qop=\"", message_qop, "\", nc=", nonce_count, ", cnonce=\"", cnonce, "\", response=\"", response_digest, "\", opaque=\"", opaque, "\r\n");
  }
 else
  {
  http_send_strings(connection, "Authorization: Digest username=\"", username, "\", realm=\"", realm, "\", nonce=\"", nonce, "\", uri=\"", request->resource, "\", response=\"", response_digest, "\", opaque=\"", opaque, "\r\n");
  }
 return HT_OK;
 }
return HT_SERVICE_UNAVAILABLE;
}

int
http_send_request(HTTP_CONNECTION *connection, HTTP_REQUEST *request)
{
const char *version = "http/1.1";
char size_buffer[32] = "";
int read_count = 0, size = 0, error = HT_OK;
HTTP_HEADER_FIELD *field_cursor = NULL;
http_reconnect(connection);
if(connection->status != HT_OK)
 {
 return connection->status;
 }
http_send_strings(connection, http_method[request->method], " ", request->resource, " ", version, "\r\n", NULL);
for(field_cursor = request->first_header_field; field_cursor != NULL; field_cursor = field_cursor->next_field)
 {
 http_send_strings(connection, field_cursor->name, ": ", field_cursor->value, "\r\n", NULL);
 }
if(connection->host != NULL)
 {
 http_send_strings(connection, "Host: ", connection->host, "\r\n", NULL);
 }
if(connection->auth_info != NULL)
 {
 http_send_authorization_header_field(connection, request);
 }
if(connection->persistent)
 {
 http_send_strings(connection, "Connection: Keep-Alive\r\n", NULL);
 }
else
 {
 http_send_strings(connection, "Connection: Close\r\n", NULL);
 }
if(request->content != NULL)
 {
 http_storage_getsize(request->content, &size);
 sprintf(size_buffer, "%d", size);
 http_send_strings(connection, "Content-Length: ", size_buffer, "\r\n", NULL);
 }
http_send_strings(connection, "\r\n", NULL);
if(request->content != NULL)
 {
 http_send_storage(connection, request->content);
 }
return connection->status;
}

int
http_add_response_header_field(HTTP_RESPONSE *response, const char *field_name, const char *field_value)
{
HTTP_HEADER_FIELD *new_header_field = NULL;
if(response == NULL || field_name == NULL || field_value == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
new_header_field = (HTTP_HEADER_FIELD *) malloc(sizeof(HTTP_HEADER_FIELD));
if(new_header_field == NULL)
 {
 return HT_MEMORY_ERROR;
 }
memset(new_header_field, 0, sizeof(HTTP_HEADER_FIELD));
new_header_field->name = strdup(field_name);
new_header_field->value = strdup(field_value);
if(new_header_field->name == NULL || new_header_field->value == NULL)
 {
 http_destroy_header_field(&new_header_field);
 return HT_MEMORY_ERROR;
 }
if(response->first_header_field == NULL)
 {
 response->first_header_field = new_header_field;
 }
else
 {
 response->last_header_field->next_field = new_header_field;
 }
new_header_field->prev_field = response->last_header_field;
response->last_header_field = new_header_field;
return HT_OK;
}

int
http_append_last_response_header_field_value(HTTP_RESPONSE *response, const char *field_value)
{
char *new_field_value = NULL;
int new_field_value_length = 0;
if(response == NULL || field_value == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
if(response->last_header_field == NULL)
 {
 return HT_ILLEGAL_OPERATION;
 }
new_field_value_length = strlen(response->last_header_field->value) + strlen(field_value);
new_field_value = (char *) realloc(response->last_header_field->value, new_field_value_length);
if(new_field_value == NULL)
 {
 return HT_MEMORY_ERROR;
 }
response->last_header_field->value = new_field_value;
strcat(response->last_header_field->value, field_value);
return HT_OK;
}

int
http_set_response_status(HTTP_RESPONSE *response, const char *status_code, const char *status_msg, const char *version)
{
char *new_status_msg = NULL, *new_version = NULL;
if(response == NULL || status_code == NULL || status_msg == NULL || version == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
if(response->status_code != 0 || response->status_msg != NULL || response->version != NULL)
 {
 return HT_ILLEGAL_OPERATION;
 }
new_status_msg = strdup(status_msg);
new_version = strdup(version);
if(new_status_msg == NULL || new_version == NULL)
 {
 free(new_status_msg);
 free(new_version);
 return HT_MEMORY_ERROR;
 }
response->status_code = (status_code[0] - '0') * 100 + (status_code[1] - '0') * 10 + (status_code[2] - '0');
response->status_msg = new_status_msg;
response->version = new_version;
return HT_OK;
}

void
http_destroy_response(HTTP_RESPONSE **response)
{
HTTP_HEADER_FIELD *field_cursor = NULL, *next_field = NULL;
if(response != NULL && *response != NULL)
 {
 free((*response)->status_msg);
 free((*response)->version);
 for(field_cursor = (*response)->first_header_field; field_cursor != NULL; field_cursor = next_field)
  {
  next_field = field_cursor->next_field;
  http_destroy_header_field(&field_cursor);
  }
 http_storage_destroy(&(*response)->content);
 free(*response);
 *response = NULL;
 }
}

int
http_create_response(HTTP_RESPONSE **response)
{
HTTP_RESPONSE *new_response = NULL;
if(response == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
new_response = (HTTP_RESPONSE *) malloc(sizeof(HTTP_RESPONSE));
if(new_response == NULL)
 {
 return HT_MEMORY_ERROR;
 }
memset(new_response, 0, sizeof(HTTP_RESPONSE));
*response = new_response;
return HT_OK;
}

#define HTTP_RECEIVING_STATUS_LINE		1
#define HTTP_RECEIVING_HEADER_FIELDS	2
#define HTTP_RECEIVING_CONTENT			3
#define HTTP_THE_DEVIL_TAKES_IT			4

int
http_receive_response_header(HTTP_CONNECTION *connection, HTTP_RESPONSE *response)
{
char read_buffer[128], *line_buffer = NULL, *new_line_buffer = NULL;
const char *status_code = NULL, *status_msg = NULL, *version = NULL;
char *field_name = NULL, *field_value = NULL, *colon = NULL, *space = NULL;
int read_count = 0, read_index = 0, content_read_count = 0, content_size = 0;
int line_index = 0, line_buffer_size = 0;
int stage = HTTP_RECEIVING_STATUS_LINE;
if(connection == NULL || response == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
if(connection->status != HT_OK)
 {
 return connection->status;
 }
while(stage <= HTTP_RECEIVING_HEADER_FIELDS && (read_count = recv(connection->socketd, read_buffer, 128, MSG_PEEK)) > 0)
 {
 read_index = 0;
 while((stage == HTTP_RECEIVING_STATUS_LINE || stage == HTTP_RECEIVING_HEADER_FIELDS) && read_index < read_count)
  {
  if(line_index >= line_buffer_size)
   {
   line_buffer_size += 128;
   new_line_buffer = (char *) realloc(line_buffer, line_buffer_size);
   if(new_line_buffer == NULL)
    {
    free(line_buffer);
    return HT_MEMORY_ERROR;
    }
   line_buffer = new_line_buffer;
   }
  if(read_buffer[read_index] == '\n')
   { 
   line_buffer[line_index] = '\0';  
   if(line_index > 0)
    {
    if(stage == HTTP_RECEIVING_STATUS_LINE)
     {
     space = strchr(line_buffer, ' ');
     if(space != NULL)
      {
      space[0] = '\0';
      version = line_buffer;
      status_code = space + 1;
      space = strchr(space + 1, ' ');
      if(space != NULL)
       {
       status_msg = space + 1;
       http_set_response_status(response, status_code, status_msg, version);
       stage = HTTP_RECEIVING_HEADER_FIELDS;
       }
      }
     }
    else if(stage == HTTP_RECEIVING_HEADER_FIELDS)
     {
     if(line_buffer[0] == ' ' || line_buffer[0] == '\t')
      {
      field_value = line_buffer + 1;
      http_append_last_response_header_field_value(response, field_value);
      }
     else
      {
      colon = strchr(line_buffer, ':');
      if(colon != NULL)
       {
       colon[0] = '\0';
       field_name = line_buffer;
       field_value = colon + 2;
       http_add_response_header_field(response, field_name, field_value);
       }
      }    
     }	
    }
   else
    {
    stage = HTTP_RECEIVING_CONTENT;
    }
   line_index = 0;
   }
  else if(read_buffer[read_index] == '\r')
   {
   }
  else
   {
   line_buffer[line_index++] = read_buffer[read_index];
   }
  read_index++;
  }
 recv(connection->socketd, read_buffer, read_index, 0);
 }
free(line_buffer);
return HT_OK;
}

int
http_receive_response_entity(HTTP_CONNECTION *connection, HTTP_RESPONSE *response)
{
char read_buffer[128];
int read_count = 0, content_read_count = 0, content_size = 0;
int stage = HTTP_RECEIVING_CONTENT;
if(connection == NULL || response == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
if(connection->status != HT_OK)
 {
 return connection->status;
 }
if(response->status_code != 204 && response->status_code != 205 && response->status_code != 304 && response->status_code > 199)
 {
 content_size = http_find_header_field_number(response, "Content-Length", LONG_MIN);
 content_read_count = 0;
 stage = HTTP_RECEIVING_CONTENT; /* start reading the body */
 }
else
 {
 stage = HTTP_THE_DEVIL_TAKES_IT; /* no content */
 }    
while(stage <= HTTP_RECEIVING_CONTENT && (read_count = recv(connection->socketd, read_buffer, 128, 0)) > 0)
 {
 if(response->content != NULL)
  {
  http_storage_write(response->content, read_buffer, read_count);
  }
 content_read_count += read_count;
 if(content_size != LONG_MIN && content_read_count >= content_size)
  {
  if(response->content != NULL)
   {
   http_storage_close(response->content);
   }
  stage = HTTP_THE_DEVIL_TAKES_IT;
  }
 }
if(connection->persistent)
 {
 if(!http_has_header_field(response, "Connection", "Keep-Alive"))
  {
  http_request_reconnection(connection);
  }
 }
return HT_OK;
}

int
http_receive_response(HTTP_CONNECTION *connection, HTTP_RESPONSE *response)
{
int error;
if((error = http_receive_response_header(connection, response)) != HT_OK)
 {
 return error;
 }
if((error = http_receive_response_entity(connection, response)) != HT_OK)
 {
 return error;
 }
return HT_OK;
}

int
http_scan_auth_request_parameters(HTTP_CONNECTION *connection, HTTP_RESPONSE *response)
{
HTTP_AUTH_PARAMETER *new_parameter = NULL;
int len = 0, end = 0;
const char *ptr = NULL;
if(connection == NULL || connection->auth_info == NULL || response == NULL)
 {
 return HT_INVALID_ARGUMENT;
 }
ptr = http_find_header_field(response, "WWW-Authenticate", NULL);
if(ptr == NULL)
 {
 return HT_RESOURCE_UNAVAILABLE;
 }
strclrws(&ptr);
len = strchrpos(ptr, ' ');
if(len == -1)
 {
 return HT_RESOURCE_UNAVAILABLE;
 }
connection->auth_info->method = strndup(ptr, len);
ptr += len + 1;
while(!end)
 {
 strclrws(&ptr);
 len = strchrpos(ptr, '=');
 if(len != -1)
  {
  new_parameter = (HTTP_AUTH_PARAMETER *) malloc(sizeof(HTTP_AUTH_PARAMETER));
  memset(new_parameter, 0, sizeof(HTTP_AUTH_PARAMETER));
  new_parameter->name = strndup(ptr, len);
  ptr += len + 1;
  strclrws(&ptr);
  len = strchrqpos(ptr, ',');
  if(len != -1)
   {
   new_parameter->value = strnunqdup(ptr, len);
   ptr += len + 1;
   }
  else
   {
   new_parameter->value = strnunqdup(ptr, strlen(ptr));
   end = 1;
   }
  http_append_auth_request_parameter(connection->auth_info, new_parameter);
  }
 else
  {
  end = 1;
  }
 }
return HT_OK;
}

const char *
http_find_header_field(HTTP_RESPONSE *response, const char *field_name, const char *default_value)
{
HTTP_HEADER_FIELD *field_cursor = NULL;
if(response == NULL || field_name == NULL)
 {
 return NULL;
 }
for(field_cursor = response->first_header_field; field_cursor != NULL; field_cursor = field_cursor->next_field)
 {
 if(strcasecmp(field_cursor->name, field_name) == 0)
  {
  return field_cursor->value;
  }
 }
return default_value;
}

long int
http_find_header_field_number(HTTP_RESPONSE *response, const char *field_name, int default_value)
{
const char *value = NULL;
value = http_find_header_field(response, field_name, NULL);
if(value == NULL)
 {
 return default_value;
 }
return strtol(value, NULL, 10);
}

int
http_has_header_field(HTTP_RESPONSE *response, const char *field_name, const char *field_value)
{
const char *value = NULL;
if(response == NULL || field_name == NULL || field_value == NULL)
 {
 return FALSE;
 }
value = http_find_header_field(response, field_name, NULL);
if(value == NULL)
 {
 return FALSE;
 }
if(strcasecmp(value, field_value) == 0)
 {
 return TRUE;
 }
return FALSE;
}

static int __http_exec_error = 0;
static char __http_exec_error_msg[256] = "";

int
http_exec_error(void)
{
return __http_exec_error;
}

const char *
http_exec_error_msg(void)
{
return __http_exec_error_msg;
}

void
http_exec_set_response_error(HTTP_RESPONSE *response)
{
if(response != NULL)
 {
 __http_exec_error = response->status_code;
 strncpy(__http_exec_error_msg, response->status_msg, 255);
 __http_exec_error_msg[255] = '\0';
 }
}

void
http_exec_set_sys_error(int error)
{
__http_exec_error = error;
switch(error)
 {
 case HT_OK: strcpy(__http_exec_error_msg, "Sys: OK"); break;
 case HT_FATAL_ERROR: strcpy(__http_exec_error_msg, "Sys: Fatal errror"); break;
 case HT_INVALID_ARGUMENT: strcpy(__http_exec_error_msg, "Sys: Invalid function argument"); break;
 case HT_SERVICE_UNAVAILABLE: strcpy(__http_exec_error_msg, "Sys: Service unavailable"); break;
 case HT_RESOURCE_UNAVAILABLE: strcpy(__http_exec_error_msg, "Sys: Resource unavailable"); break;
 case HT_MEMORY_ERROR: strcpy(__http_exec_error_msg, "Sys: Memory error"); break;
 case HT_NETWORK_ERROR: strcpy(__http_exec_error_msg, "Sys: Network error"); break;
 case HT_ILLEGAL_OPERATION: strcpy(__http_exec_error_msg, "Sys: Illegal operation"); break;
 case HT_HOST_UNAVAILABLE: strcpy(__http_exec_error_msg, "Sys: Host not found"); break;
 case HT_IO_ERROR: strcpy(__http_exec_error_msg, "Sys: I/O Error"); break;
 }
}

http_exec(HTTP_CONNECTION *connection, int method, const char *resource, 
          HTTP_EVENT_HANDLER on_request_header, HTTP_EVENT_HANDLER on_request_entity, 
		  HTTP_EVENT_HANDLER on_response_header, HTTP_EVENT_HANDLER on_response_entity, void *data)
{
HTTP_REQUEST *request = NULL;
HTTP_RESPONSE *response = NULL;
int error = HT_OK;
if((error = http_create_request(&request, method, resource)) != HT_OK
|| (error = http_create_response(&response)) != HT_OK)
 {
 http_destroy_request(&request);
 http_destroy_response(&response);
 http_exec_set_sys_error(error);
 return error;
 }
if(on_request_header != NULL)
 {
 if((error = on_request_header(connection, request, response, data)) != HT_OK)
  {
  http_destroy_request(&request);
  http_destroy_response(&response);
  http_exec_set_sys_error(error);
  return error;
  }
 }
if(on_request_entity != NULL)
 {
 if((error = on_request_entity(connection, request, response, data)) != HT_OK)
  {
  http_destroy_request(&request);
  http_destroy_response(&response);
  http_exec_set_sys_error(error);
  return error;
  }
 }
if((error = http_send_request(connection, request)) != HT_OK
|| (error = http_receive_response_header(connection, response)) != HT_OK)
 {
 http_destroy_request(&request);
 http_destroy_response(&response);
 http_exec_set_sys_error(error);
 return error;
 }
if(response->status_code == 401)
 {
 if(connection->auth_info != NULL && connection->auth_info->method == NULL)
  {
  if(http_scan_auth_request_parameters(connection, response) == HT_OK)
   {
   http_receive_response_entity(connection, response);
   http_destroy_response(&response);
   if((error = http_create_response(&response)) != HT_OK
   || (error = http_send_request(connection, request)) != HT_OK
   || (error = http_receive_response_header(connection, response)) != HT_OK)
    {
    http_destroy_request(&request);
    http_destroy_response(&response);
    http_exec_set_sys_error(error);
    return error;
    }
   }
  }
 }
if(on_response_header != NULL)
 {
 if((error = on_response_header(connection, request, response, data)) != HT_OK)
  {
  http_receive_response_entity(connection, response);
  http_exec_set_sys_error(error);
  return error;
  }
 }
http_receive_response_entity(connection, response);
if(on_response_entity != NULL)
 {
 if((error = on_response_entity(connection, request, response, data)) != HT_OK)
  {
  http_exec_set_sys_error(error);
  return error;
  }
 }
http_exec_set_response_error(response);
http_destroy_request(&request);
http_destroy_response(&response);
return error;
}
