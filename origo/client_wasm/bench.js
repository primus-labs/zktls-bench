const puppeteer = require('puppeteer');

(async () => {
  const browser = await puppeteer.launch({
    headless: true,
    args: ["--no-sandbox",
      "--disable-dev-shm-usage",
      "--disable-gpu",
      "--enable-features=SharedArrayBuffer",
      "--ignore-certificate-errors",
    ]
  });
  const page = await browser.newPage();

  let taskCompleted = false;
  page.on('console', msg => {
    console.log(`Console >> ${msg.text()}`);
    // console.log(`${msg.text()}`);
    if (msg.text().includes('proof successfully generated') > 0) {
      // console.log(`${msg.text()}`);
      taskCompleted = true;
    }
    else if (msg.text().includes("WASM Memory Usage") > 0) {
      console.log(`${msg.text()}`);
    }
    else if (msg.text().includes("worker thread") > 0 && msg.text().includes("seconds") > 0) {
      console.log(`${msg.text()}`);
    }
  });

  const url = "https://127.0.0.1:38082"
  await page.goto(url);

  await new Promise(resolve => {
    const checkInterval = setInterval(() => {
      if (taskCompleted) {
        clearInterval(checkInterval);
        resolve();
      }
    }, 500);
  });

  await browser.close();
})();

