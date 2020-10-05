# ImageTagger

This was project 1 of Semester 1 2019 Computer Systems (COMP30023).

The goal was to build a simple game where two local players (same device) would take turns tagging an image with words that can be used to describe it, and if two words are the same then they would progress to the next round. 

Detailed instructions for the game are provided in `image_tagger.html`.

The game server was implemented using socket programming in C and could be interacted with using HTTP protocols.

---
Running the game:

To run, navigate into the directory where all files are located and compile using `make all`.

The game can be run using the resulting `image_tagger` executable file in the command line. 

This file takes two parameters:
- a specified server IP address
- a specified server PORT number

After running the executable, open your browser of choice and navigate to <IP address>:<PORT number>. Thus the game then begins.
