# Particle-WoL
The device portion of a Particle Photon based Wake on Lan implementation. Allows you to wake and ping your WoL enabled devices from anywhere in the world by calling Particle cloud functions.
### Usage:
Simply flash to your Particle Photon after setting it up on your home network, then use the two cloud functions
#### wakeHost():
Call wakehost to wake a computer. Use an argument in the form of "IP;MAC_ADDRESS" of the computer you want to wake. E.G. "192.168.1.1;00:00:00:00:00:01". 
#### pingHost():
Call pinghost to ping a local computer to check whether it's up. Takes an IP address as an argument. Pinghost is called automatically after waking a computer to verify the success of the wakeup. 

### Credits: 
Based on Pedro Pombeiro's Particle WOL code.
  
     
     
      
       
        
         
         
          
That's it!
