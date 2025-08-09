import net from 'net'

const PIPE_PATH = '\\\\.\\pipe\\sensor_data';

let dataClient: net.Socket;


const client = net.connect(PIPE_PATH, () => {
  console.log('Connected to C++ service');
  // 发送Server hello的握手消息
  client.write(JSON.stringify({
    type: 'SERVER_HELLO',
  }));
  // logger.info("data")
});

client.on('data', (data) => {
  console.log('Received from C++:', data.toString());
  const message = JSON.parse(data.toString());
  if(message.type === 'CLIENT_HELLO') {
    console.log('Server hello received');
    // 申请一个数据通道
    client.write(JSON.stringify({
      type: 'SETUP_DATA_CHANNEL',
    }));
  }
  if(message.type === 'DATA_CHANNEL_READY') {
    console.log('Data channel setup received ',message);
    // 申请一个数据通道
    dataClient = net.connect(message.payload.DataPipeName, () => {
      console.log('Connected to C++ data service');
    });
    dataClient.on('data', (data) => {
      console.log('Received from C++ data service:', data.toString());
    })

  }


  // client.write('finish');
});

client.on('error', (err) => {
  console.error('Pipe error:', err.message);
});