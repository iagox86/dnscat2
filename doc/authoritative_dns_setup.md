# Authoritative DNS Server Setup

dnscat2 runs in two modes — through a direct connection to your server, and through the DNS hierarchy.  The second mode requires you to make your server an authoritative DNS server, which traditionally handles the DNS query and converts the url to an IP address.

# Setup on Namecheap

Before you start, you will need a server that is running dnscat2 and a domain name that you own.  For our example, let's suppose our dnscat2 server has an IP address of 42.42.42.42 and our domain name is "company"

We will go through setting up an authoritative DNS server on Namecheap, a popular domain registrar.  Other services such as GoDaddy have a similar setup process.

### Step 1: Sign in

![Sign in](http://i.imgur.com/LILdHvC.png)

### Step 2: Click on the "manage" button for your domain

![Manage button](http://i.imgur.com/MqviES4.png)

### Step 3: Click "Advanced DNS" on the left site of the domain navbar

![Advanced DNS](http://i.imgur.com/dLxn3oe.png)

### Step 4: Click the "EDIT" button on the right of "Domain Nameserver Type"

Remember to set this to Custom!

![Domain Nameserver Type](http://i.imgur.com/TNSKC8V.png)

### Step 5: After seting the Server Type to custom fill in the details of your nameservers

![Custom Server Type](http://i.imgur.com/FiL8dtM.png)

### Step 6: Now it is time to add your personal DNS servers so click the button "ADD NEW" to the right

![Adding personal DNS servers](http://i.imgur.com/nKhautV.png)

### Step 7: Click the custom button twice and fill in your DNS details

Don't forget to click on the "Save Changes" button

![Adding personal DNS servers](http://i.imgur.com/pBZ5lrU.png)

You may verify that your authoritative DNS server is correctly setup by running `sudo nc -vv -l -u -p53` on your dnscat2 server, and then sending a DNS query for your domain name. If your server detects a UDP packet, then you have successfully setup your authoritative DNS server!
