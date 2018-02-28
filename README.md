# FTPServer_Client
FTP server and client. FTP server uses select() for IO multiplexing within a single process rather than fork()ing for each client.
