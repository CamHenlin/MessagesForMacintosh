# Messages For Macintosh
Messages for Macintosh is a software suite for classic Macintosh (System 2.0 through MacOS 9.2.2) systems to interoperate with Apple iMessages via a familiar interface, with [supporting software](https://github.com/CamHenlin/imessagegraphqlserver) running on a newer Mac computer.

![messages for macintosh](https://henlin.net/images/messagesformacintosh.png)

## How to use Messages for Macintosh
There are two setup guides for Messages for Macintosh:

- [the user-facing setup guide](https://henlin.net/), for users not interested in exploring the development workflow. This is the article you want if you just want to use your old Mac to chat.
- [the developer-facing setup guide](https://henlin.net/), which explains how to set Messages for Macintosh up in such a way that you can make modifications to its different pieces

## Technologies
Messages for Macintosh is built on a lot of technologies. The READMEs of each of these is each worth familiarizing yourself with if you would like to contribute to Messages for Macintosh:

- [Retro68](https://github.com/autc04/Retro68) - a GCC-based cross compilation env for classic Macintosh systems
- [Nuklear Quickdraw](https://github.com/CamHenlin/nuklear-quickdraw) - a heavily modified, classic Macintosh-specific version of [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) allowing a simple way to provide GUI services
- [CoprocessorJS](https://github.com/CamHenlin/coprocessor.js) - a library that allows us to handle nodejs workloads sent over a serial port to talk to [supporting software](https://github.com/CamHenlin/imessagegraphqlserver)
- [serialperformanceanalyzer](https://github.com/CamHenlin/serialperformanceanalyzer) - used to analyze the performance of many different parts of the application during its development lifecycle

## Limitations / areas for improvement
Messages for Macintosh is not perfect, but it is usable. Here are some known limitations and things that could be improved:

- 10 conversations at a time, with the ability to open a new conversation if you know the recipient's address book entry. Nuklear supports scrolling and we could likely add more to the list, but the performance may begin degrading on 68000-based systems.
- Up to the most 15 recent messages are displayed in your selected chat. No pagination. Nuklear supports scrolling so we could add more messages, but again it may be slow on 68000-based systems if we do that
- No image / emoji support (although emojis are generally translated to text)
- Performance can still be improved. We set a good baseline here, but there is still more that can be done. There are lots of traces in the codebase supporting [serialperformanceanalyzer](https://github.com/CamHenlin/serialperformanceanalyzer) if someone would like to take a stab at further improving performance.
- All updates are based on polling and can sometimes be slow

## Pull requests and issues welcome
If you make any improvements or run into any issues, please feel free to bring them back to this repo with pull requests or issue reports. Both are welcome and will help Messages for Macintosh to be more useful in the future. 

## Animated demo
Here is a short demo of the classic Mac app in operation:

![messages for macintosh demo animation](https://henlin.net/images/messagesformacdemo.gif)
