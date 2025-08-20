#include "ESPWebDAV.h"
#include <SdFat.h>

// Sections are copied from ESP8266Webserver

// ------------------------
String ESPWebDAV::getMimeType(String path) {
// ------------------------
	if(path.endsWith(".html")) return "text/html";
	else if(path.endsWith(".htm")) return "text/html";
	else if(path.endsWith(".css")) return "text/css";
	else if(path.endsWith(".txt")) return "text/plain";
	else if(path.endsWith(".js")) return "application/javascript";
	else if(path.endsWith(".json")) return "application/json";
	else if(path.endsWith(".png")) return "image/png";
	else if(path.endsWith(".gif")) return "image/gif";
	else if(path.endsWith(".jpg")) return "image/jpeg";
	else if(path.endsWith(".ico")) return "image/x-icon";
	else if(path.endsWith(".svg")) return "image/svg+xml";
	else if(path.endsWith(".ttf")) return "application/x-font-ttf";
	else if(path.endsWith(".otf")) return "application/x-font-opentype";
	else if(path.endsWith(".woff")) return "application/font-woff";
	else if(path.endsWith(".woff2")) return "application/font-woff2";
	else if(path.endsWith(".eot")) return "application/vnd.ms-fontobject";
	else if(path.endsWith(".sfnt")) return "application/font-sfnt";
	else if(path.endsWith(".xml")) return "text/xml";
	else if(path.endsWith(".pdf")) return "application/pdf";
	else if(path.endsWith(".zip")) return "application/zip";
	else if(path.endsWith(".gz")) return "application/x-gzip";
	else if(path.endsWith(".appcache")) return "text/cache-manifest";

	return "application/octet-stream";
}




// ------------------------
String ESPWebDAV::urlDecode(const String& text)	{
// ------------------------
	String decoded = "";
	char temp[] = "0x00";
	unsigned int len = text.length();
	unsigned int i = 0;
	while (i < len)	{
		char decodedChar;
		char encodedChar = text.charAt(i++);
		if ((encodedChar == '%') && (i + 1 < len))	{
			temp[2] = text.charAt(i++);
			temp[3] = text.charAt(i++);
			decodedChar = strtol(temp, NULL, 16);
		}
		else {
			if (encodedChar == '+')
				decodedChar = ' ';
			else
				decodedChar = encodedChar;  // normal ascii char
		}
		decoded += decodedChar;
	}
	return decoded;
}

// ------------------------
String ESPWebDAV::urlToUri(String url)	{
// ------------------------
	if(url.startsWith("http://"))	{
		int uriStart = url.indexOf('/', 7);
		return url.substring(uriStart);
	}
	else
		return url;
}

// ------------------------
bool ESPWebDAV::isClientWaiting() {
// ------------------------
	return server->hasClient();
}

// ------------------------
void ESPWebDAV::handleClient(String blank) {
// ------------------------
	processClient(&ESPWebDAV::handleRequest, blank);
}

// ------------------------
void ESPWebDAV::rejectClient(String rejectMessage) {
// ------------------------
	processClient(&ESPWebDAV::handleReject, rejectMessage);
}

// ------------------------
void ESPWebDAV::processClient(THandlerFunction handler, String message) {
// ------------------------
	// Check if a client has connected
	client = server->available();
	if(!client)
		return;

	// Wait until the client sends some data
	while(!client.available())
		delay(1);
	
	// reset all variables
	_chunked = false;
	_responseHeaders = String();
	_contentLength = CONTENT_LENGTH_NOT_SET;
	method = String();
	uri = String();
	contentLengthHeader = String();
	depthHeader = String();
	hostHeader = String();
	destinationHeader = String();

	// extract uri, headers etc
	if(parseRequest())
		// invoke the handler
		(this->*handler)(message);
		
	// finalize the response
	if(_chunked)
		sendContent("");

	// send all data before closing connection
	client.flush();
	// close the connection
	client.stop();
}

// ------------------------
bool ESPWebDAV::parseRequest() {
// ------------------------
	// Read the first line of HTTP request
	String req = client.readStringUntil('\r');
	client.readStringUntil('\n');
	
	// First line of HTTP request looks like "GET /path HTTP/1.1"
	// Retrieve the "/path" part by finding the spaces
	int addr_start = req.indexOf(' ');
	int addr_end = req.indexOf(' ', addr_start + 1);
	if (addr_start == -1 || addr_end == -1) {
		return false;
	}

	method = req.substring(0, addr_start);
	uri = urlDecode(req.substring(addr_start + 1, addr_end));
	DBG_PRINT("method: "); DBG_PRINT(method); DBG_PRINT(" url: "); DBG_PRINTLN(uri);
	
	// parse and finish all headers
	String headerName;
	String headerValue;
	
	while(1) {
		req = client.readStringUntil('\r');
		client.readStringUntil('\n');
		if(req == "") 
			// no more headers
			break;
			
		int headerDiv = req.indexOf(':');
		if (headerDiv == -1)
			break;
		
		headerName = req.substring(0, headerDiv);
		headerValue = req.substring(headerDiv + 2);
		DBG_PRINT("\t"); DBG_PRINT(headerName); DBG_PRINT(": "); DBG_PRINTLN(headerValue);
		
		if(headerName.equalsIgnoreCase("Host"))
			hostHeader = headerValue;
		else if(headerName.equalsIgnoreCase("Depth"))
			depthHeader = headerValue;
		else if(headerName.equalsIgnoreCase("Content-Length"))
			contentLengthHeader = headerValue;
		else if(headerName.equalsIgnoreCase("Destination"))
			destinationHeader = headerValue;
	}
	
	return true;
}

// ------------------------
void ESPWebDAV::sendHeader(const String& name, const String& value, bool first) {
// ------------------------
	String headerLine = name + ": " + value + "\r\n";

	if (first)
		_responseHeaders = headerLine + _responseHeaders;
	else
		_responseHeaders += headerLine;
}

// ------------------------
void ESPWebDAV::send(String code, const char* content_type, const String& content) {
// ------------------------
	String header;
	_prepareHeader(header, code, content_type, content.length());

	client.write(header.c_str(), header.length());
	if(content.length())
		sendContent(content);
}

// ------------------------
void ESPWebDAV::_prepareHeader(String& response, String code, const char* content_type, size_t contentLength) {
// ------------------------
	response = "HTTP/1.1 " + code + "\r\n";

	if(content_type)
		sendHeader("Content-Type", content_type, true);
	
	if(_contentLength == CONTENT_LENGTH_NOT_SET)
		sendHeader("Content-Length", String(contentLength));
	else if(_contentLength != CONTENT_LENGTH_UNKNOWN)
		sendHeader("Content-Length", String(_contentLength));
	else if(_contentLength == CONTENT_LENGTH_UNKNOWN) {
		_chunked = true;
		sendHeader("Accept-Ranges","none");
		sendHeader("Transfer-Encoding","chunked");
	}
	sendHeader("Connection", "close");

	response += _responseHeaders;
	response += "\r\n";
}

// ------------------------
void ESPWebDAV::sendContent(const String& content) {
// ------------------------
	const char * footer = "\r\n";
	size_t size = content.length();
	
	if(_chunked) {
		char * chunkSize = (char *) malloc(11);
		if(chunkSize) {
			sprintf(chunkSize, "%x%s", size, footer);
			client.write(chunkSize, strlen(chunkSize));
			free(chunkSize);
		}
	}
	
	client.write(content.c_str(), size);
	
	if(_chunked) {
		client.write(footer, 2);
		if (size == 0) {
			_chunked = false;
		}
	}
}

// ------------------------
void ESPWebDAV::sendContent_P(PGM_P content) {
// ------------------------
	const char * footer = "\r\n";
	size_t size = strlen_P(content);
	
	if(_chunked) {
		char * chunkSize = (char *) malloc(11);
		if(chunkSize) {
			sprintf(chunkSize, "%x%s", size, footer);
			client.write(chunkSize, strlen(chunkSize));
			free(chunkSize);
		}
	}
	
	client.write_P(content, size);
	
	if(_chunked) {
		client.write(footer, 2);
		if (size == 0) {
			_chunked = false;
		}
	}
}

// ------------------------
void ESPWebDAV::setContentLength(size_t len)	{
// ------------------------
	_contentLength = len;
}

// ------------------------
size_t ESPWebDAV::readBytesWithTimeout(uint8_t *buf, size_t bufSize) {
// ------------------------
	int timeout_ms = HTTP_MAX_POST_WAIT;
	size_t numAvailable = 0;
	while(!(numAvailable = client.available()) && client.connected() && timeout_ms--) 
		delay(1);

	if(!numAvailable)
		return 0;

	return client.read(buf, bufSize);
}

// ------------------------
size_t ESPWebDAV::readBytesWithTimeout(uint8_t *buf, size_t bufSize, size_t numToRead) {
// ------------------------
	int timeout_ms = HTTP_MAX_POST_WAIT;
	size_t numAvailable = 0;
	
	while(((numAvailable = client.available()) < numToRead) && client.connected() && timeout_ms--) 
		delay(1);

	if(!numAvailable)
		return 0;

	return client.read(buf, bufSize);
}

// ------------------------
// Try to add HTTP file list and upload/download
// ------------------------

// ------------------------
void ESPWebDAV::handleFileList() {
// ------------------------

    SdFile root;
    if (!root.open("/", O_READ)) {
        send("500", "text/plain", "Cannot open root");
        return;
    }

    SdFile child;
    char fname[256];
    uint32_t fileSize;
    dir_t dir;
    tm tmStr;
    time_t t2t;
    tm *gTm;
    char buf[20];

    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='utf-8'><title>SD Card Files</title>";
    html += "<style>";
    html += "table { border-collapse: collapse; width: 100%; }";
    html += "th, td { border: 1px solid #ccc; padding: 8px; text-align: left; cursor: pointer; }";
    html += "th { background-color: #eee; }";
    html += "</style>";
    html += "<script>"
            "function sortTable(n) {"
            "  var table, rows, switching, i, x, y, shouldSwitch, dir, switchcount = 0;"
            "  table = document.getElementById('fileTable');"
            "  switching = true;"
            "  dir = 'asc';"
            "  while (switching) {"
            "    switching = false;"
            "    rows = table.rows;"
            "    for (i = 1; i < (rows.length - 1); i++) {"
            "      shouldSwitch = false;"
            "      x = rows[i].getElementsByTagName('TD')[n];"
            "      y = rows[i + 1].getElementsByTagName('TD')[n];"
            "      var xContent = x.innerHTML.toLowerCase();"
            "      var yContent = y.innerHTML.toLowerCase();"
            "      if (n == 1) { xContent = parseInt(xContent); yContent = parseInt(yContent); }"
            "      if (dir == 'asc') { if (xContent > yContent) { shouldSwitch = true; break; } }"
            "      else { if (xContent < yContent) { shouldSwitch = true; break; } }"
            "    }"
            "    if (shouldSwitch) {"
            "      rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);"
            "      switching = true; switchcount++;"
            "    } else {"
            "      if (switchcount == 0 && dir == 'asc') { dir = 'desc'; switching = true; }"
            "    }"
            "  }"
            "}"
            "</script>";
    html += "</head><body>";
    html += "<h1>Files on SD Card</h1>";
    html += "<table id='fileTable'><tr>";
    html += "<th onclick='sortTable(0)'>Filename</th>";
    html += "<th onclick='sortTable(1)'>Size (bytes)</th>";
    html += "<th onclick='sortTable(2)'>Last Modified</th></tr>";

    while (child.openNext(&root, O_READ)) {
        child.getName(fname, sizeof(fname));

        if (!child.isDir()) {
            fileSize = child.fileSize();

            // Get last modified timestamp using WebDAV method
            child.dirEntry(&dir);

            tmStr.tm_hour = FAT_HOUR(dir.lastWriteTime);
            tmStr.tm_min = FAT_MINUTE(dir.lastWriteTime);
            tmStr.tm_sec = FAT_SECOND(dir.lastWriteTime);
            tmStr.tm_year = FAT_YEAR(dir.lastWriteDate) - 1900;
            tmStr.tm_mon = FAT_MONTH(dir.lastWriteDate) - 1;
            tmStr.tm_mday = FAT_DAY(dir.lastWriteDate);

            t2t = mktime(&tmStr);
            gTm = gmtime(&t2t);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gTm);

            html += "<tr>";
            html += "<td><a href='/download?name=" + String(fname) + "'>" + String(fname) + "</a></td>";
            html += "<td>" + String(fileSize) + "</td>";
            html += "<td>" + String(buf) + "</td>";
            html += "</tr>";
        }
        child.close();
    }

    root.close();

    html += "</table></body></html>";
    send("200", "text/html", html);

}

void ESPWebDAV::handleFileDownload() {
    // Extract filename from URL: /download?name=filename.txt
    int idx = uri.indexOf("?name=");
    if (idx == -1) {
        send("400", "text/plain", "No filename specified");
        return;
    }

    String filename = urlDecode(uri.substring(idx + 6));

    SdFile root;
    if (!root.open("/", O_READ)) {
        send("500", "text/plain", "Cannot open root directory");
        return;
    }

    SdFile file;
    if (!file.open(&root, filename.c_str(), O_READ)) {
        send("404", "text/plain", "File not found");
        root.close();
        return;
    }

    // Send HTTP headers for download
    String header = "HTTP/1.1 200 OK\r\n";
    header += "Content-Type: application/octet-stream\r\n";
    header += "Content-Length: " + String(file.fileSize()) + "\r\n";
    header += "Content-Disposition: attachment; filename=\"" + filename + "\"\r\n";
    header += "Connection: close\r\n\r\n";
    client.write(header.c_str(), header.length());

    // Stream file in 256-byte chunks
    uint8_t buf[256];
    int n;
    while ((n = file.read(buf, sizeof(buf))) > 0) {
        client.write(buf, n);
    }

    file.close();
    root.close();
}

void ESPWebDAV::handleStatusPage() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<title>ESP8266 Status</title>";
    html += "<style>body{font-family:Arial; margin:20px;} table{border-collapse:collapse;} td,th{border:1px solid #ccc; padding:6px 10px;}</style>";
    html += "</head><body>";
    html += "<h2>ESP8266 Status</h2>";
    html += "<table>";

    html += "<tr><th>WiFi SSID</th><td>" + WiFi.SSID() + "</td></tr>";
    html += "<tr><th>IP Address</th><td>" + WiFi.localIP().toString() + "</td></tr>";
    html += "<tr><td>MAC Address</td><td>" + WiFi.macAddress() + "</td></tr>";
    html += "<tr><th>Signal Strength (RSSI)</th><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
    int ch = WiFi.channel();
    int freq = 2407 + ch * 5;
    html += "<tr><td>WiFi Channel</td><td>" + String(ch) + "</td></tr>";
    html += "<tr><td>WiFi Frequency</td><td>" + String(freq) + " MHz</td></tr>";
    html += "<tr><th>Free Heap</th><td>" + String(ESP.getFreeHeap()) + " bytes</td></tr>";

    // uptime in seconds
    unsigned long uptime = millis() / 1000;
    html += "<tr><th>Uptime</th><td>" + String(uptime) + " sec</td></tr>";

    html += "</table>";
    html += "<p><a href='/files'>Browse Files</a></p>";
    html += "</body></html>";

    send("200", "text/html", html);
}
