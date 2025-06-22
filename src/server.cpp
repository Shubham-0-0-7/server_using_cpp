#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>
#include <thread>

using namespace std;

string read_file(const string &filename)
{
    ifstream file(filename);
    if (!file)
        return "";

    ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

bool write_file(const string &filename, const string &content)
{
    ofstream file(filename);
    if (!file)
        return false;
    
    file << content;
    return true;
}

bool ends_with(const string &value, const string &suffix)
{
    if (suffix.size() > value.size())
        return false;
    return equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

string get_content_type(const string &path)
{
    if (ends_with(path, ".html"))
        return "text/html";
    if (ends_with(path, ".css"))
        return "text/css";
    if (ends_with(path, ".js"))
        return "application/javascript";
    if (ends_with(path, ".png"))
        return "image/png";
    if (ends_with(path, ".jpg") || ends_with(path, ".jpeg"))
        return "image/jpeg";
    if (ends_with(path, ".gif"))
        return "image/gif";
    if (ends_with(path, ".json"))
        return "application/json";
    return "text/plain";
}

map<string, string> parse_headers(const string &request)
{
    map<string, string> headers;
    istringstream stream(request);
    string line;
    
    // Skip the first line (request line)
    getline(stream, line);
    
    while (getline(stream, line) && line != "\r")
    {
        size_t colon_pos = line.find(':');
        if (colon_pos != string::npos)
        {
            string key = line.substr(0, colon_pos);
            string value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t\r") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            headers[key] = value;
        }
    }
    
    return headers;
}

string extract_body(const string &request)
{
    size_t body_start = request.find("\r\n\r\n");
    if (body_start != string::npos)
    {
        return request.substr(body_start + 4);
    }
    return "";
}

string url_decode(const string &str)
{
    string result;
    for (size_t i = 0; i < str.length(); ++i)
    {
        if (str[i] == '%' && i + 2 < str.length())
        {
            int hex_value;
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &hex_value);
            result += static_cast<char>(hex_value);
            i += 2;
        }
        else if (str[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

map<string, string> parse_form_data(const string &body)
{
    map<string, string> form_data;
    istringstream stream(body);
    string pair;
    
    while (getline(stream, pair, '&'))
    {
        size_t equals_pos = pair.find('=');
        if (equals_pos != string::npos)
        {
            string key = url_decode(pair.substr(0, equals_pos));
            string value = url_decode(pair.substr(equals_pos + 1));
            form_data[key] = value;
        }
    }
    
    return form_data;
}

string handle_post_request(const string &path, const string &body, const map<string, string> &headers)
{
    // Handle different POST endpoints
    if (path == "/submit")
    {
        // Parse form data
        map<string, string> form_data = parse_form_data(body);
        
        // Log the received data
        cout << "Received POST data:\n";
        for (const auto &pair : form_data)
        {
            cout << "  " << pair.first << " = " << pair.second << "\n";
        }
        
        // Create a response
        ostringstream response_body;
        response_body << "<html><body>";
        response_body << "<h1>Form Submitted Successfully!</h1>";
        response_body << "<h2>Received Data:</h2><ul>";
        
        for (const auto &pair : form_data)
        {
            response_body << "<li><strong>" << pair.first << ":</strong> " << pair.second << "</li>";
        }
        
        response_body << "</ul>";
        response_body << "<a href='/'>Go Back</a>";
        response_body << "</body></html>";
        
        return response_body.str();
    }
    else if (path == "/api/data")
    {
        // Handle JSON API endpoint
        cout << "Received JSON data: " << body << "\n";
        
        // Simple JSON response
        return "{\"status\": \"success\", \"message\": \"Data received\", \"timestamp\": \"" + to_string(time(nullptr)) + "\"}";
    }
    else if (path == "/upload")
    {
        // Simple file upload simulation
        string filename = "uploads/data_" + to_string(time(nullptr)) + ".txt";
        
        if (write_file(filename, body))
        {
            return "<html><body><h1>File uploaded successfully!</h1><p>Saved as: " + filename + "</p><a href='/'>Go Back</a></body></html>";
        }
        else
        {
            return "<html><body><h1>Upload failed!</h1><a href='/'>Go Back</a></body></html>";
        }
    }
    
    // Default response for unknown POST endpoints
    return "<html><body><h1>POST endpoint not found</h1><p>Path: " + path + "</p><a href='/'>Go Back</a></body></html>";
}

void handle_client(int client_fd) {
    char buffer[8192] = {0};
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }

    string request(buffer, bytes_read);
    cout << "Request:\n" << request << "\n";

    istringstream request_stream(request);
    string method, path, http_version;
    request_stream >> method >> path >> http_version;

    string response_body;
    string status_line;
    string content_type;

    if (method == "GET") {
        if (path == "/") path = "/index.html";
        string filename = "." + path;
        response_body = read_file(filename);

        if (response_body.empty()) {
            status_line = "HTTP/1.1 404 Not Found\r\n";
            response_body = "<h1>404 Not Found</h1>";
            content_type = "text/html";
        } else {
            status_line = "HTTP/1.1 200 OK\r\n";
            content_type = get_content_type(path);
        }
    } else if (method == "POST") {
        auto headers = parse_headers(request);
        string body = extract_body(request);
        response_body = handle_post_request(path, body, headers);
        status_line = "HTTP/1.1 200 OK\r\n";
        content_type = (path == "/api/data") ? "application/json" : "text/html";
    } else if (method == "PUT") {
        string body = extract_body(request);
        string filename = "." + path;
        if (write_file(filename, body)) {
            status_line = "HTTP/1.1 200 OK\r\n";
            response_body = "<h1>PUT Success</h1>";
        } else {
            status_line = "HTTP/1.1 500 Internal Server Error\r\n";
            response_body = "<h1>PUT Failed</h1>";
        }
        content_type = "text/html";
    } else if (method == "DELETE") {
        string filename = "." + path;
        if (remove(filename.c_str()) == 0) {
            status_line = "HTTP/1.1 200 OK\r\n";
            response_body = "<h1>Deleted Successfully</h1>";
        } else {
            status_line = "HTTP/1.1 404 Not Found\r\n";
            response_body = "<h1>File Not Found</h1>";
        }
        content_type = "text/html";
    } else {
        status_line = "HTTP/1.1 405 Method Not Allowed\r\n";
        response_body = "<h1>405 Method Not Allowed</h1>";
        content_type = "text/html";
    }

    string response = status_line +
                      "Content-Type: " + content_type + "\r\n"
                      "Content-Length: " + to_string(response_body.length()) + "\r\n"
                      "Connection: close\r\n"
                      "\r\n" +
                      response_body;

    send(client_fd, response.c_str(), response.length(), 0);
    close(client_fd);
}


int main()
{
    // 1. Create a socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        cerr << "Socket creation failed.\n";
        return 1;
    }

    // Allow socket reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Define server address
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    // 3. Bind socket to address
    if (::bind(server_fd, (sockaddr *)&address, sizeof(address)) < 0)
    {
        cerr << "Bind failed.\n";
        return 1;
    }

    // 4. Start listening
    if (listen(server_fd, 5) < 0)
    {
        cerr << "Listen failed.\n";
        return 1;
    }

    cout << "Server is running at http://localhost:8080\n";

    // 5. Accept clients
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (true)
    {
        int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            cerr << "Accept failed.\n";
            continue;
        }

        // Handle each client in a new thread
        thread(handle_client, client_fd).detach();
    }
    close(server_fd);
    return 0;
}