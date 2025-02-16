const puppeteer = require('puppeteer');

(async () => {
  const args = process.argv.slice(2)
  const program = args[0];
  const party = args[1];
  const ip = args[2];
  const port = args[3];
  const requestSize = args[4];
  const responseSize = args[5];

  const browser = await puppeteer.launch({
    headless: true,
    args: ["--no-sandbox",
      "--disable-dev-shm-usage",
      "--disable-gpu",
      "--enable-features=SharedArrayBuffer",
    ]
  });
  const page = await browser.newPage();

  let taskCompleted = false;
  page.on('console', msg => {
    // console.log(`Console >> ${msg.text()}`);
    console.log(`${msg.text()}`);
    if (msg.text().includes('DONE:') > 0) {
      // console.log(`${msg.text()}`);
      taskCompleted = true;
    }
  });

  const url = "http://127.0.0.1:8000/index.html"
  await page.goto(url);
  const params = {
    program: program,
    party: party,
    ip: ip,
    port: port,
    requestSize: requestSize,
    responseSize: responseSize,
  }
  await page.evaluate((params) => {
    document.querySelector('#program').value = params.program;
    document.querySelector('#party').value = params.party;
    document.querySelector('#ip').value = params.ip;
    document.querySelector('#port').value = params.port;
    document.querySelector('#requestSize').value = params.requestSize;
    document.querySelector('#responseSize').value = params.responseSize;
    return `Set ${JSON.stringify(params)} OK`;
  }, params);

  await page.waitForSelector('button#start', { visible: true });
  await page.click('button#start');


  await new Promise(resolve => {
    const checkInterval = setInterval(() => {
      if (taskCompleted) {
        clearInterval(checkInterval);
        resolve();
      }
    }, 500);
  });

  await page.evaluate(() => {
    if (window.performance && window.performance.memory) {
      const memStat = {
        "usedJSHeapSize": window.performance.memory.usedJSHeapSize,
        "totalJSHeapSize": window.performance.memory.totalJSHeapSize
      };
      console.log("memStat:", JSON.stringify(memStat))
    } else {
      console.log('Memory API is not supported.');
    }
  });

  await browser.close();
})();

