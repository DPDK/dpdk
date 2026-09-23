const http = require('http');
const https = require('https');
const url = require('url');

// Create an HTTP server to send test HTTP requests
const sendHttpRequest = (hostname, port, path, method) => {
  const options = {
    hostname: hostname,
    port: port,
    path: path,
    method: method,
    headers: {
      'User-Agent': 'Node.js HTTP Test Client',
      'Content-Type': 'application/json'
    }
  };

  const req = http.request(options, (res) => {
    console.log(`HTTP Response: ${res.statusCode} ${res.statusMessage}`);
    res.on('data', (chunk) => {
      console.log(`Body: ${chunk}`);
    });
  });

  req.on('error', (error) => {
    console.error(`Error: ${error}`);
  });

  req.end();
};

// Test HTTP requests (adjust the IP and port based on your setup)
const testUrl = 'localhost';  // DPDK application is listening on localhost
const testPort = 80;          // Standard HTTP port

const testRequests = [
  { method: 'GET', path: '/' },
  { method: 'POST', path: '/test' },
  { method: 'GET', path: '/about' },
];

// Send test HTTP requests
testRequests.forEach(({ method, path }) => {
  sendHttpRequest(testUrl, testPort, path, method);
});
