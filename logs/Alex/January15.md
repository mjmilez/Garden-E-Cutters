### January 15th
Setting it up
* labeled "rover" and "base" at the bottom of the board. 
* added a sticker on the chip that we are not going to move that labels rover for easier visibility

Going through the User Guide for the XLR kit.

## setting up the base station
there was a youtube video explaining how so iteratively hooked up the radio and lr antenna to base station and rover

## setting up Ucenter 
* had to get the proper 2013 Visual Code from microsoft architecture to get UCenter to run on my computer

## going outside
* Matthew and I set up the rover and base station and went out in front of the REITZ to get sattelite exposure and test the accuracy
*WE RAN INTO PROBLEMS*
* one, the data type never changed from 3D on the rover. While its devent location accuracy it did not meet the parameters of what we need for the assignment. 
* We were unable to ascertain wether or not the base and reciever have established a connection
* gained some familiarity with the UCenter Platform
* determined that at least partially the base has been configured to be the base station and the rover to be the rover, their transmittions line up with such

## current unfinished problems
* the rover never recieved RTCM messages (the correction messages neccessary for accuracy and the point of RTK)
* the base after 90 seconds stop setting its position this meant the average location for the base never got down to centimeter level (only got to 2.11 meters) that it needs to function properly. 
* why is the base not working? , some say that it takes 30 minutes to fully find the bases fixed location but our base stopped even calculating it's location after 90 seconds. 
* is the problem with the rover that the base doesn't have it's location fixed?
