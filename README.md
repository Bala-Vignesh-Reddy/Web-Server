# Multithreaded Proxy Web Server with LPU cache

This is the project which tell us about.. how the Multi-threaded webserver with LRU cache are working under the hood..

### Concepts Learned while building are:
- Multithreading - helps cpu to execute concurrently
- Semaphores - mechanism used to control access to shared resource.. prevent race condition
- Mutex Lock - takes care that only one thread get the access to the resource
- Sockets - endpoint communication between two machines.. 
- LRU Cache (Least Recently Used) - mechanism that removes the least recently accessed items first
- pthreads - POSIX standard api for creating and managing threads

### Features
- HTTP/1.0 and HTTP/1.1 support
- Parallel request handling
- LRU caching mechanism
- Mutex and semaphore usage to conqueror race condition
- Only supports GET request

![flowchart](<docs/Multithreaded web server.png>)

### Project Description
- Understanding 
    - the working of server that is request from local computer to the server 
    - handling of multiple requests from clients
    - understanding the mechanism for LRU cache.
    - using semaphore and mutex_lock to avoid race condition 
- working 
    - User sends the request to the proxy server.. 
    - it checks whether the asked url is present in cache or not.. 
        - if yes.. then returns back the body of the requested url.. 
        - it no.. then sends the request further to the main server.. getting the information 
    - adds the request to the cache and send the body and info to the client.. 
    Note: for every request different thread is been created and it is connected with the socket which is the bridge between the server and the client.. 

![client](docs/clients.png)

- limitations
    - only accepts the get request
    - doesn't support the https request.. 
    - doesn't support the latest sites.. cause they uses the http2 and http3.. 
- tech specs
    - Max Clients - 10
    - Default port - 8080 (can change during running)
    - Cache size - 10MB

### Build and Run 
```bash
git clone https://github.com/Bala-Vignesh-Reddy/Web-Server.git 
cd Web-Server
make
./proxy <port_num>
```

### Contributing
[Back to top](https://github.com/Lovepreet-Singh-LPSK/MultiThreadedProxyServerClient#index)

