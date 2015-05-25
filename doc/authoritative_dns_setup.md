# Authoritative DNS Server Setup

dnscat2 runs in two modes — through a direct connection to your server, and through the DNS hierarchy.  The second mode requires you to make your server an authoritative DNS server, which traditionally handles the DNS query and converts the url to an IP address.

# Setup on Namecheap

Before you start, you will need a server that is running dnscat2 and a domain name that you own.  For our example, let's suppose our dnscat2 server has an IP address of 42.42.42.42 and our domain name is example.com

We will go through setting up an authoritative DNS server on Namecheap, a popular domain registrar.  Other services such as GoDaddy have a similar setup process.

### Step 1: Sign in and click on "Manage Domains"

![Manage Domains Image](http://i.imgur.com/FVhxTX3l.png)

### Step 2: Click on your domain name

In this case, we click on example.com

![Cick on your domain name](http://i.imgur.com/m5lmfnIh.png)

### Step 3: Click "Transfer DNS to Webhost" on the left navbar, and specify the subdomains for your DNS server.

Make sure "Specify Custom DNS Servers" is selected.  Add whichever subdomains you'd like your DNS servers to be.  Note that two such subdomains need to be specified.  If you are unsure, add the `ns1` and `ns2` subdomains, as we have in the example.  Click on "Save Changes"

![Specify subdomain](http://i.imgur.com/M6RSIG9h.png)

### Step 4: Click "Nameserver Registration", and specify the IP address of your DNS server.

Add the IP address of your dnscat2 server to the corresponding subdomains that you specified earlier.  Note that one IP address can serve multiple subdomains.

![Nameserver registration](http://i.imgur.com/iyXFKNuh.png)

### Step 5: Done!  Wait at least 48 hours to allow the changes to come into effect.

You may verify that your authoritative DNS server is correctly setup by running `sudo nc -vv -l -u -p53` on your dnscat2 server, and then sending a DNS query for your domain name. If your server detects a UDP packet, then you have successfully setup your authoritative DNS server!
