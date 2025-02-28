import { providers } from '@reclaimprotocol/attestor-core/lib/providers'
import { TLSProtocolVersion } from '@reclaimprotocol/tls/lib/types'

const parseCertificate = require('@reclaimprotocol/tls/lib/utils/parse-certificate');
const mockVerifyCertificateChain = (): void => { };
parseCertificate.verifyCertificateChain = mockVerifyCertificateChain

import { createClaimOnAttestor } from '@reclaimprotocol/attestor-core/lib/client'
import { ZKEngine } from '@reclaimprotocol/zk-symmetric-crypto'
import { AttestorClient } from '@reclaimprotocol/attestor-core/lib/client/utils/client-socket'
import { logger, uint8ArrayToStr, extractApplicationDataFromTranscript, getTranscriptString } from '@reclaimprotocol/attestor-core/lib/utils'
import { decryptTranscript } from '@reclaimprotocol/attestor-core/lib/server'
import { assertValidClaimSignatures } from '@reclaimprotocol/attestor-core'


export async function startProver(
  attestorIp: string = 'localhost', attestorPort: number = 12345,
  zkEngine: ZKEngine = 'gnark', reqLength: number = 1024, repLength: number = 1024,
  mockUrl = 'https://localhost:17777/me', tlsVersion: TLSProtocolVersion = "TLS1_3") {

  providers.http.additionalClientOptions = {
    // we need to disable certificate verification for testing purposes
    verifyServerCertificate: false,
    supportedProtocolVersions: [tlsVersion]
  }


  const wsServerUrl = `ws://${attestorIp}:${attestorPort}/ws`
  const client = new AttestorClient({
    logger: logger.child({ client: 1 }),
    url: wsServerUrl,
  })
  await client.waitForInit()

  const user = 'adhiraj'
  const claimUrl = mockUrl
  const start = +new Date()
  const result = await createClaimOnAttestor({
    name: 'http',
    params: {
      url: claimUrl,
      method: 'POST',
      body: '',
      headers: {
        'replength': repLength.toString()
      },
      responseRedactions: [
        {
          jsonPath: 'd'
        }
      ],
      responseMatches: [
        {
          type: 'contains',
          value: '0'
        }
      ]
    },
    secretParams: {
      authorisationHeader: `Bearer ${user}`
    },
    ownerPrivateKey: '0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80',
    client: client,
    zkEngine,
  })

  if (result.error) {
    console.error('error in creating claim', result.error)
    return
  }

  // 
  // Now, anybody (including you) can verify this claim by using the signature & the claim.
  // 
  // // this will throw an error if the claim or result is invalid
  // await assertValidClaimSignatures(result)

  // // If you'd like to actually view the transcript of the claim that the attestor saw & made a claim on, 
  // // you can do so by accessing the transcript field in the result. 
  // // This is a good way to verify your code isn't leaking any sensitive information.
  // const transcript = result.request!.transcript
  // const decTranscript = await decryptTranscript(
  //   transcript, logger, zkEngine,
  //   result.request?.fixedServerIV!, result.request?.fixedClientIV!
  // )

  // // convert the transcript to a string for easy viewing
  // const transcriptStr = getTranscriptString(decTranscript)
  // console.log('receipt:\n', transcriptStr)

  var memeory = 0
  if (typeof window === 'undefined') {
    memeory = process.memoryUsage().rss / 1024
  }

  const stat = {
    ak: zkEngine,
    cost: +new Date() - start,
    memory: memeory
  }
  console.log(`DONE:${JSON.stringify(stat)}`);

  await client.terminateConnection()
}

async function test() {
  var attestorIp = "localhost"
  var attestorPort = 12345
  var zkEngine: ZKEngine = 'gnark'
  var reqLength = 1024
  var repLength = 1024
  var mockUrl = `https://localhost:17777/me`
  var tlsVersion: TLSProtocolVersion = 'TLS1_3'
  if (typeof window !== 'undefined') {
    attestorIp = "127.0.0.1"
    attestorPort = 12345
    zkEngine = 'snarkjs'
    reqLength = 1024
    repLength = 1024
    // note: if run desktop-browser-chrome.
    // - use `python https_server.py` start the server 
    // - set `–unsafely-treat-insecure-origin-as-secure=ws://127.0.0.1:12345`
    mockUrl = `https://127.0.0.1:17777/me`
    tlsVersion = 'TLS1_3'
    // on browser
    console.log('on browser, see browser/index.html')
    return;
  } else {
    // on node
    // [ip] [port] [gnark|snarkjs] [request-length] [response-length]
    const args = process.argv.slice(2)
    if (args.length > 0) {
      attestorIp = args[0]
    }
    if (args.length > 1) {
      attestorPort = parseInt(args[1])
    }
    if (args.length > 2) {
      if (args[2] == 'snarkjs') zkEngine = 'snarkjs';
    }
    if (args.length > 3) {
      reqLength = parseInt(args[3])
    }
    if (args.length > 4) {
      repLength = parseInt(args[4])
    }
    if (args.length > 5) {
      if (args[5] == 'TLS1_2') tlsVersion = 'TLS1_2';
    }
  }

  await startProver(attestorIp, attestorPort, zkEngine, reqLength, repLength, mockUrl, tlsVersion)
}
test()

