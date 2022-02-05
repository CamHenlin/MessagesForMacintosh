require('cross-fetch/polyfill')
const ApolloClient = require('apollo-boost').ApolloClient;
const InMemoryCache = require('apollo-cache-inmemory').InMemoryCache;
const createHttpLink = require('apollo-link-http').createHttpLink;
const gql = require('graphql-tag')

// TEST_MODE can be turned on or off to prevent communications with the Apollo iMessage Server running on your modern Mac
const TEST_MODE = false
const DEBUG = false

const defaultOptions = {
  watchQuery: {
    fetchPolicy: 'no-cache',
    errorPolicy: 'ignore',
  },
  query: {
    fetchPolicy: 'no-cache',
    errorPolicy: 'ignore',
  },
}

let client

const widthFor12ptFont = [
  0,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  8,
  10,
  10,
  10,
  0,
  10,
  10,
  10,
  11,
  11,
  9,
  11,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  10,
  4,
  6,
  7,
  10,
  7,
  11,
  10,
  3,
  5,
  5,
  7,
  7,
  4,
  7,
  4,
  7,
  8,
  8,
  8,
  8,
  8,
  8,
  8,
  8,
  8,
  8,
  4,
  4,
  6,
  8,
  6,
  8,
  11,
  8,
  8,
  8,
  8,
  7,
  7,
  8,
  8,
  6,
  7,
  9,
  7,
  12,
  9,
  8,
  8,
  8,
  8,
  7,
  6,
  8,
  8,
  12,
  8,
  8,
  8,
  5,
  7,
  5,
  8,
  8,
  6,
  8,
  8,
  7,
  8,
  8,
  6,
  8,
  8,
  4,
  6,
  8,
  4,
  12,
  8,
  8,
  8,
  8,
  6,
  7,
  6,
  8,
  8,
  12,
  8,
  8,
  8,
  5,
  5,
  5,
  8,
  8
]

// this is tied to mac_main.c's message window max width
const MAX_WIDTH = 304
const SPACE_WIDTH = widthFor12ptFont[32]
let canStart = false
let hasNewMessages = false

const getNextWordLength = (word) => {

  let currentWidth = 0

  for (const char of word.split(``)) {

    let currentCharWidth = widthFor12ptFont[char.charCodeAt()]

    if (isNaN(currentCharWidth)) {

    currentCharWidth = 1;
    }

    currentWidth += currentCharWidth
  }

  return currentWidth
}

const shortenText = (text) => {

  let outputText = ``
  let currentWidth = 0

  for (const word of text.split(` `)) {

    let currentWordWidth = getNextWordLength(word)

    if (currentWidth + currentWordWidth + SPACE_WIDTH > MAX_WIDTH) {

    outputText = `${outputText}ENDLASTMESSAGE`
    currentWidth = 0

    // okay, but what if the word itself is greater than max width?
    if (currentWordWidth > MAX_WIDTH) {

      let splitWordWidth = 0

      for (const char of word.split(``)) {

        let currentCharWidth = widthFor12ptFont[char.charCodeAt()]

        if (isNaN(currentCharWidth)) {
      
          currentCharWidth = 1;
        }

        if (splitWordWidth + currentCharWidth > MAX_WIDTH) {

          outputText = `${outputText}ENDLASTMESSAGE`
          splitWordWidth = 0
        }

        splitWordWidth += currentCharWidth
        outputText = `${outputText}${char}`
        }

        currentWidth += splitWordWidth
        
        continue
      }
    }

    currentWidth += currentWordWidth + SPACE_WIDTH
    outputText = `${outputText} ${word}`
  }

  return outputText
}

const MAX_ROWS = 16

const splitMessages = (messages) => {

  let firstMessage = true

  if (!messages) {

    return `no messages ENDLASTMESSAGE`
  }

  for (const message of messages) {

    if (firstMessage) {

      let tempMessageOutput = `${message.chatter}: ${message.text}`

      tempMessageOutput = shortenText(tempMessageOutput)
      messageOutput = tempMessageOutput
    } else {

      let tempMessageOutput = `${message.chatter}: ${message.text}`

      tempMessageOutput = shortenText(tempMessageOutput)
      messageOutput = `${messageOutput}ENDLASTMESSAGE${tempMessageOutput}`
    }

    firstMessage = false
  }


  if (messageOutput.split(`ENDLASTMESSAGE`).length > MAX_ROWS) {

    messageOutput = messageOutput.split(`ENDLASTMESSAGE`)

    let newMessageOutput = []

    for (let i = messageOutput.length; i > messageOutput.length - MAX_ROWS; i--) {

      newMessageOutput.unshift(messageOutput[i])
    }

    messageOutput = newMessageOutput.join(`ENDLASTMESSAGE`)
  } 

  lastMessageOutput = messageOutput

  return messageOutput
}

const parseChatsToFriendlyNameString = (chats) => {

  let friendlyNameStrings = ``

  if (!chats) {

    return ``
  }

  for (let chat of chats) {

    friendlyNameStrings = `${friendlyNameStrings},${chat.friendlyName.replace(/,/g, '')}`
  }

  friendlyNameStrings = friendlyNameStrings.substring(1, friendlyNameStrings.length)

  return friendlyNameStrings
}

let lastMessageOutput

let TEST_MESSAGES = [
  {chatter: `friend 1`, text: `my super fun text message`},
  {chatter: `me`, text: `some cool old thing I said earlier`},
  {chatter: `friend 2`, text: `this message is not relevant to the conversation! not at all :(`},
  {chatter: `friend 1`, text: `i watch star wars in reverse order`},
  {chatter: `me`, text: `https://github.com/CamHenlin/MessagesForMacintosh https://github.com/CamHenlin/MessagesForMacintosh`},
  {chatter: `friend 3`, text: `i'm just catching up`},
  {chatter: `friend 3`, text: `nobody chat for a minute`},
  {chatter: `friend 2`, text: `hang on`},
  {chatter: `friend 1`, text: `no`}
]

const TEST_CHATS = [
  {friendlyName: `my group chat 1`, name: `my group chat 1`},
  {friendlyName: `friend 1`, name: `friend 1`},
  {friendlyName: `friend 4`, name: `friend 4`},
  {friendlyName: `boss`, name: `boss`},
  {friendlyName: `friend 3`, name: `friend 3`},
  {friendlyName: `restaurant`, name: `restaurant`}
]

if (TEST_MODE) {

  setInterval(() => {

    TEST_MESSAGES = TEST_MESSAGES.concat({chatter: `friend 1`, text: Math.random().toString(36).replace(/[^a-z]+/g, '').substr(0, 64)})
  }, 10000)
}

let storedArgsAndResults = {
  getMessages: {
    args: {},
    output: {}
  },
  hasNewMessagesInChat: {
    args: {},
    output: {}
  },
  getChats: {
    args: {},
    output: {}
  },
  getChatCounts: {
    args: {},
    output: {}
  }
}

// this is our private interface, meant to communicate with our GraphQL server and fill caches
// we want everything cached as much as possible to cut down on perceived perf issues on the 
// classic Macintosh end
class iMessageGraphClientClass {

  async getMessages (chatId, page, fromInterval) {

    storedArgsAndResults.getMessages.args = {
      chatId,
      page
    }

    if (TEST_MODE) {

      return splitMessages(TEST_MESSAGES)
    }

    if (DEBUG) {

      console.log(`get messages for chat ID: ${chatId}`)
    }

    let result

    try {
    
      result = await client.query({
        query: gql`query getMessages {
            getMessages(chatId: "${chatId}", page: "${page}") {
                chatter
                text
            }
        }`
      })
    } catch (error) {

      console.log(`getMessages: error with apollo query`)
      console.log(error)

      return
    }

    let messages = result.data.getMessages

    let currentLastMessageOutput = `${lastMessageOutput}`

    storedArgsAndResults.getMessages.output = splitMessages(messages)

    if (!hasNewMessages && fromInterval) {

      hasNewMessages = currentLastMessageOutput !== storedArgsAndResults.getMessages.output

      if (hasNewMessages) {

        console.log(`got new message. previous message was:`)
        console.log(currentLastMessageOutput)
        console.log(`new message set is:`)
        console.log(storedArgsAndResults.getMessages.output)
      }
    }

    return
  }

  async hasNewMessagesInChat () {

    if (!hasNewMessages) {

      return `false`
    } else {

      hasNewMessages = false
    }

    return `true`
  }

  async sendMessage (chatId, message) {

    if (TEST_MODE) {

      TEST_MESSAGES = TEST_MESSAGES.concat({chatter: `me`, text: message})

      return splitMessages(TEST_MESSAGES)
    }

    let result

    try {

      message = encodeURIComponent(message)

      result = await client.query({
        query: gql`query sendMessage {
            sendMessage(chatId: "${chatId}", message: "${message}") {
                chatter
                text
            }
        }`
      })
    } catch (error) {

      console.log(`sendMessage: error with apollo query`)
      console.log(error)

      return
    }

    let messages = result.data.sendMessage

    storedArgsAndResults.getMessages.output = splitMessages(messages)

    return storedArgsAndResults.getMessages.output
  }

  async getChats () {


    if (DEBUG) {

      console.log(`getChats`)
    }

    if (TEST_MODE) {

      return parseChatsToFriendlyNameString(TEST_CHATS)
    }

    let result

    try {
    
      result = await client.query({
        query: gql`query getChats {
            getChats {
                name
                friendlyName
            }
        }`
      })
    } catch (error) {

      console.log(`getChats: error with apollo query`)
      console.log(error)

      return
    }

    let chats = result.data.getChats

    storedArgsAndResults.getChats.output = parseChatsToFriendlyNameString(chats)

    if (DEBUG) {

      console.log(`getChats complete`)
      console.log(storedArgsAndResults.getChats.output)
    }

    return
  }

  async getChatCounts () {

    if (DEBUG) {

      console.log(`getChatCounts`)
    }

    if (TEST_MODE) {

      return parseChatsToFriendlyNameString(TEST_CHATS)
    }

    let result

    try {
    
      result = await client.query({
        query: gql`query getChatCounts {
            getChatCounts {
                friendlyName
                count
            }
        }`
      })
    } catch (error) {

      console.log(`getChatCounts: error with apollo query`)
      console.log(error)

      return
    }

    let chats = result.data.getChatCounts

    if (!chats) {

      return
    }

    let friendlyNameStrings = ``
  
    if (chats.length === 0) {
  
      return
    }
  
    for (let chat of chats) {
  
      friendlyNameStrings = `${friendlyNameStrings},${chat.friendlyName.replace(/,/g, '')}:::${chat.count}`
    }
  
    // remove trailing comma
    friendlyNameStrings = friendlyNameStrings.substring(1, friendlyNameStrings.length)

    storedArgsAndResults.getChatCounts.output = friendlyNameStrings

    if (DEBUG) {

      console.log(`got chat counts`)
      console.log(friendlyNameStrings)
    }

    return
  }

  setIPAddress (IPAddress) {

    console.log(`instantiate apolloclient with uri ${IPAddress}:4000/`)

    if (TEST_MODE) {

      canStart = true

      return `success`
    }

    try {

      client = new ApolloClient({
        uri: `${IPAddress}:4000/`,
        cache: new InMemoryCache(),
        link: new createHttpLink({
          uri: `${IPAddress}:4000/`
        }),
        defaultOptions
      });
    } catch (err) {
      console.log(`error instantiating the ApolloClient`)
      console.log(err)

      return `failure`
    }

    console.log(`return success`)

    canStart = true

    return `success`
  }
}

let iMessageGraphClient = new iMessageGraphClientClass()

// provide the public interface
class iMessageClient {

  constructor () {

    // kick off an update interval
    setInterval(async () => {

      let intervalDate = new Date().toISOString()

      console.log(`${intervalDate}: run interval`)
    
      if (!canStart) {
    
        console.log(`${intervalDate}: can't start yet`)
    
        return
      }

      if (DEBUG) {

        console.log(`${intervalDate}: running...`)
      }

      try {
    
        if (Object.keys(storedArgsAndResults.getMessages.args).length > 0) {

          console.log(`${intervalDate}: interval: get messages for ${storedArgsAndResults.getMessages.args.chatId}`)
          await iMessageGraphClient.getMessages(storedArgsAndResults.getMessages.args.chatId, storedArgsAndResults.getMessages.args.page, true)
        }
      
        console.log(`${intervalDate}: interval: getchats`)
        await iMessageGraphClient.getChats()
        console.log(`${intervalDate}: interval: getchatcounts`)
        await iMessageGraphClient.getChatCounts()
      } catch (error) {

        console.log(`${intervalDate}: caught error when running interval`)
        console.log(error)
      }
    
      if (DEBUG) {

        console.log(`${intervalDate}: complete!`)
      }
    }, 3000)
  }

  async getMessages (chatId, page) {

    console.log(`iMessageClient.getMessages`)

    if (storedArgsAndResults.getMessages.args.chatId !== chatId || storedArgsAndResults.getMessages.args.page !== page) {

      await iMessageGraphClient.getMessages(chatId, page, false)
    }

    console.log(`iMessageClient.getMessages, return:`)
    console.log(storedArgsAndResults.getMessages.output)

    return storedArgsAndResults.getMessages.output
  }

  async hasNewMessagesInChat (chatId) {

    console.log(`iMessageClient.hasNewMessagesInChat`)

    let returnValue = await iMessageGraphClient.hasNewMessagesInChat(chatId)

    console.log(`iMessageClient.hasNewMessagesInChat, return:`)
    console.log(returnValue)

    return returnValue
  }

  async sendMessage (chatId, message) {

    console.log(`iMessageClient.sendMessage(${chatId}, ${message})`)

    const messages = await iMessageGraphClient.sendMessage(chatId, message)

    return messages
  }

  async getChats () {

    console.log(`iMessageClient.getChats`)
    
    if (Object.keys(storedArgsAndResults.getChats.output).length === 0) {

      await iMessageGraphClient.getChats()
    }

    console.log(`iMessageClient.getChats, return:`)
    console.log(storedArgsAndResults.getChats.output)

    return storedArgsAndResults.getChats.output
  }

  getChatCounts () {

    console.log(`iMessageClient.getChatCounts, prestored return:`)
    console.log(storedArgsAndResults.getChatCounts.output)

    return storedArgsAndResults.getChatCounts.output
  }

  setIPAddress (IPAddress) {

    console.log(`iMessageClient.setIPAddress`)

    return iMessageGraphClient.setIPAddress(IPAddress)
  }
}

module.exports = iMessageClient

