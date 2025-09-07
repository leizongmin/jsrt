import { Hono } from 'hono';
import { serve } from '@hono/node-server';

const app = new Hono();

// GET / - 主页路由
app.get('/', (c) => {
  return c.html(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>Hono Node.js Example</title>
      <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .container { max-width: 600px; margin: 0 auto; }
        .endpoint { background: #f5f5f5; padding: 10px; margin: 10px 0; border-radius: 5px; }
        button { background: #007acc; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; }
        button:hover { background: #005a9e; }
        #result { margin-top: 20px; padding: 10px; background: #e8f4fd; border-radius: 5px; }
      </style>
    </head>
    <body>
      <div class="container">
        <h1>🔥 Hono Node.js Example</h1>
        <p>This is a simple Hono framework example running on Node.js in jsrt runtime.</p>
        
        <h2>Available Endpoints:</h2>
        <div class="endpoint">
          <strong>GET /</strong> - This page
        </div>
        <div class="endpoint">
          <strong>POST /api/hello</strong> - Echo API endpoint
        </div>
        
        <h2>Test API:</h2>
        <button onclick="testAPI()">Test POST /api/hello</button>
        <div id="result"></div>
        
        <script>
          async function testAPI() {
            try {
              const response = await fetch('/api/hello', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name: 'World', message: 'Hello from Hono!' })
              });
              const data = await response.json();
              document.getElementById('result').innerHTML = 
                '<h3>Response:</h3><pre>' + JSON.stringify(data, null, 2) + '</pre>';
            } catch (error) {
              document.getElementById('result').innerHTML = 
                '<h3>Error:</h3><pre>' + error.message + '</pre>';
            }
          }
        </script>
      </div>
    </body>
    </html>
  `);
});

// POST /api/hello - API 端点
app.post('/api/hello', async (c) => {
  try {
    const body = await c.req.json();
    return c.json({
      success: true,
      message: 'Hello from Hono API!',
      received: body,
      timestamp: new Date().toISOString(),
      server: 'Hono on Node.js (jsrt)',
    });
  } catch (error) {
    return c.json(
      {
        success: false,
        error: 'Invalid JSON body',
        message: 'Please send valid JSON data',
      },
      400
    );
  }
});

// 404 处理
app.notFound((c) => {
  return c.json({ error: 'Not Found', path: c.req.path }, 404);
});

// 错误处理
app.onError((err, c) => {
  console.error('Server error:', err);
  return c.json({ error: 'Internal Server Error' }, 500);
});

const port = process.env.PORT || 3000;

console.log(`🔥 Hono server starting on port ${port}`);
console.log(`📍 Open http://localhost:${port} in your browser`);

serve({
  fetch: app.fetch,
  port: port,
});

console.log(`✅ Server is running on http://localhost:${port}`);
