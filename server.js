
var http = require('http');
http.createServer((req, res) => {
  console.log('Echo Fake IP');
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('10.1.0.2');
  res.end();
}).listen(9000);

