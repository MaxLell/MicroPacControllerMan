These ressources may be used as inspiration for the implementation of the task. If you have any other information available, please also do your own research on how this may be implemented.

# Stanford Pacman AI Projekt
## Task and Description
https://stanford-cs221.github.io/spring2025/assignments/pacman/index.html
https://stanford.edu/~cpiech/cs221/homework/prog/pacman/pacman.html

## potential solutions (These were not tried)
https://github.com/saurabh-pal-dn/Pacman-AI
https://github.com/Abdelaal495/Multi-Agent-Pac-Man

## Paper
https://cs229.stanford.edu/proj2017/final-reports/5241109.pdf


# Custom Implementation
## Github
https://github.com/jjwarren44/Pacman-AI
https://github.com/allenmonkey970/NEAT-Pacman 
Please also research other projects for that matter.

## Code Bullet
- He built Pacman the game for himself: This can be found here: https://github.com/Code-Bullet/PacmanGame
- He then used NEAT for then training the Neural network. For this there is no repo available (or at least I did not find any). I will only provide you with the transcript of his youtube video, which might still bury some insight.
    - I think what is worth mentioning that he trained the Pacman Bot with 'Left', 'Right', 'Forward', 'Backward' rather then the global 'North', 'South', 'West', 'East' direction input. This is not only mentioned in his example but also in other examples, which I found online.

### Transcript
 Get that trash out of here. Who would want to watch that garbage? This is YouTube. You guys aren't here to read especially when you've got such a talented artist as your host. Hey guys, It's Evan from Co-Ed Bullet. Here, you've probably met my writing before, but as great as white text on a black background is, I thought it would be about time to upgrade. I know it's a bit early to start, thank you, my Patreon supporters, but I can't help it. Thank you guys so much. Thanks to you, I'm able to go out by this awesome mic and start actually talking in my videos. For what anyway, it is Christmas, ladies and gentlemen.

Mostly gentlemen, actually. Ninety-five percent gentlemen. Just made myself sad. Sorry. Yes, it’s Pac-Man time. Let me tell you about the struggle, which is the making of Pac-Man, and why it took me so long to make. So, you have had the right guys. First of all, Pac-Man is hard. It’s more complicated of a game than you think.

Compared to other games like the asteroids, where the AI only had to learn to like point and shoot and also avoid Pac-Man is so much more complicated.

So, when I just threw the AI that it had no idea what it is doing, it wasn't learning. They didn't know when to eat the ghosts, when to not eat the ghosts. Sometimes it was allowed to, sometimes it wasn't, and it also had to learn to navigate the maze at the same time. It was a mess; it was not happening.

So, I had to hold his hand a little bit. I split the learning into three stages. The first stage was with no ghosts, no big dots, just Pac-Man learning to navigate the maze. The only way it could die is if it stopped moving. What doesn't eat any pellets within a certain amount of time after about 20 generations of learning, we introduced the ghosts, but we kept the big dots out of there. Now that Pac-Man has learned to navigate the maze, it can focus on learning how to Evolve to avoid the ghosts, and since there are no big dots, Pac-Man isn't confused by sometimes being able to eat them, and sometimes they kill him. Then after another 40 generations of learning, we introduced the big dots, and we can finally finish the game. In theory, in practice, it’s always a bit different, but that’s the idea.

Dear, also, I’m sorry about my drawing skills, but I need to put something on the screen. I know my amazing artistic skills might be distracting for some, but I’m sure you guys can bear with it.

Alright, let’s jump into it. When the AI is first thrown into the game, they are useless. They absolutely suck. The best the first generation could do is an AI, which always turns right, which is fantastic. Unless you intend on playing the game of Pac-Man. So, safe to say that didn’t last very long. By generation 2, it has figured out how to turn both left and right, which is great, but it still gets stuck. I hope you’re enjoying the new layer. It’s Got the news on that controlling the Pac-Man in the bottom-right corner, so you can watch it evolve as Pac-Man's behavior involved. I think it's pretty cool. As you can imagine, playing Pac-Man without any ghosts is quite simple. Well it's simple for a human, but surprisingly hard for an AI, so I had to change the

controls up to make it far more intuitive for them. Instead of the controls turning Pac-Man in absolute directions like north, south, east, or west, like they normally are, I had to change it so it's relative to Pac-Man. So, for example, turn left or right this way, they could have simple rules such as, when Pac-Man sees a wall in front of it, it turns left. You can see how complicated the neural network is getting, even though we haven't added any ghosts or any big dots yet. Okay, so by generation 18, it's getting pretty good. It still actually doesn't complete the maze, but it gets up to around 200, which I'm pretty happy with. As you would expect.

The AIs have no idea what's going on, they've just been suddenly introduced to evil ghosts which will try and eat them.

So, fair to say they're not doing super. It's kind of like introducing an invasive species; it's evolve or perish. But slowly and surely, the AIs learn to avoid the ghosts. Also, I hope you notice the awesome sprites are made for Pac-Man. And the ghosts—well, I’ll be honest, that is copied Pac-Man, but I have original designs for the ghosts. I got an angry Blinky, a girl Pinky, which is all the personality. She gets Clyde, who is just uselessly adorable, and I kind of lost interest by the time I made Inky, so he's just kind of like, angry with a smile.

Pac-Man is actually a very difficult game without the big dots, without the energizer dots, even for a human. So, the fact that these guys are getting scores of about a hundred and fifty is fairly impressive. I think so. So, I'm fairly happy with that. Let's add the dots and see how they go.

They actually adapt very quickly to the introduction of the big dots, and as you would expect, the scores they are getting skyrocket. This guy's just going to town on the ghosts, getting some revenge for the past 40 generations of torture. Here we are, generation 73, and spoiler: look, this is the one that finally does it. Finally, after two weeks of trying and failing, with blood, sweat, and tears, finally got it to finish the game. There’s no way I could make a video and have it not finish the game. It’s only got four dots left. Get the bottom two, get the top two, and then it gets confused and dies.

Hey guys, I hope you appreciated the mic a bunch of people asking for it, which is why I did it. And I think it’s a lot more fun than just white text on a black background. But if you guys prefer that, tell me in the comments. And if you guys prefer me with a microphone, also tell me in the comments. It’s really appreciated. I know I already thanked them, but I’ll send Another huge thanks to my Patreon, but really, allowing me to do this. YouTube's still being a pain in the ass and hasn't actually approved my channel yet, which is kind of frustrating. But thanks to my Patreon, I can actually keep going while I'm thanking people. I would like to thank my buddies over at my Discord channel who helped me make the awesome intro at the start. I say, help me make they did everything.

I added some music to it. It's awesome. Thank you guys so much.

One topic of outsourcing my work to you guys: my channel art is trash. It is not going to cut it. You guys have seen my attempt at art, and it's fair to say that I need a bit of help. So, if anyone wants to be a bloody chair viewed, head over to my Discord channel to post any art there; that would be phenomenal. Thank you guys so much.

I've decided to do live Q&As every week. I put it out to my Patreon, and they reckon the best time for them was any.

Day from 8:30 till 9, so I'm gonna choose Sunday night 8:30 to 9:00. That's Australian Eastern Standard Time. So, if you guys can make it, that'd be awesome. Send me a question, I'll try and answer as many people as I possibly can within that time. As per usual, I do have a beautiful fail video for you guys. I hope you enjoy it.

Where to go if you enjoy the fail video? Have a good one! See you next time.

