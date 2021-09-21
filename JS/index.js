require('cross-fetch/polyfill')
const ApolloClient = require('apollo-boost').ApolloClient;
const InMemoryCache = require('apollo-cache-inmemory').InMemoryCache;
const createHttpLink = require('apollo-link-http').createHttpLink;
const gql = require('graphql-tag')

const defaultOptions = {
  watchQuery: {
    fetchPolicy: 'no-cache',
    errorPolicy: 'ignore',
  },
  query: {
    fetchPolicy: 'no-cache',
    errorPolicy: 'all',
  },
}

const client = new ApolloClient({
  uri: 'http://10.0.1.166:4000/',
  cache: new InMemoryCache(),
  link: new createHttpLink({
    uri: 'http://10.0.1.166:4000/'
  }),
  defaultOptions
});


class iMessageClient {

  async getMessages (chatId, page) {

    console.log(`get messages for caht ID: ${chatId}`)

    let result = await client.query({
      query: gql`query getMessages {
          getMessages(chatId: "${chatId}", page: "${page}") {
              chatter
              text
          }
      }`
    })

    let messages = result.data.getMessages
    let messageOutput = ``
    const maxPerLine = 20000
    console.log(`return messages from get messages:`)
    console.log(messages)

    let firstMessage = true

    if (!messages) {

      return ``
    }

    for (const message of messages) {

      if (firstMessage) {
        messageOutput = `${message.chatter}: ${message.text}`
      } else {
        messageOutput = `${messageOutput}ENDLASTMESSAGE${message.chatter}: ${message.text}`
      }

      firstMessage = false
    }

    return messageOutput
  }


  async sendMessage (chatId, message) {

    console.log(`send messages for caht ID: ${chatId} ${message}`)

    let result = await client.query({
      query: gql`query sendMessage {
          sendMessage(chatId: "${chatId}", message: "${message}") {
              chatter
              text
          }
      }`
    })

    let messages = result.data.getMessages
    let messageOutput = ``
    const maxPerLine = 20000
    console.log(`return messages from send messages:`)
    console.log(messages)

    let firstMessage = true

    if (!messages) {

      return ``
    }

    for (const message of messages) {

      if (firstMessage) {
        messageOutput = `${message.chatter}: `
      } else {
        messageOutput = `${messageOutput}ENDLASTMESSAGE${message.chatter}: `
      }

      firstMessage = false
    }

    return messageOutput
  }

  async getChats () {

    let result = await client.query({
      query: gql`query getChats {
          getChats {
              name
              friendlyName
          }
      }`
    })

    let chats = result.data.getChats
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
}

module.exports = iMessageClient

