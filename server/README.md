# How to run server

## Install ruby

- On Linux: <http://watir.com/guides/ruby/>

- On MacOS:

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew install readline rbenv ruby-install
rbenv init
echo 'export PATH="$HOME/.rbenv/bin:$PATH"' >> ~/.zshrc
echo 'eval "$(rbenv init -)"' >> ~/.zshrc
source ~/.zshrc
rbenv install 2.7.3
rbenv local 2.7.3
ruby -v
```

## Run with auto script

- Add the commands you want to run in `commands.txt`

- Then start the server, passing in the name of the `commands.txt` file using `--auto-script` option

```bash
ruby ./dnscat2.rb --auto-script="commands.txt"
```

- Whenever a client connects, the server will keeps selecting and running a random command in the `commands.txt` file. It will sleep a bit between 2 commands.

- The maximum sleep time can be changed in the `server/controller/session.rb` file, at this line:

```ruby
max_wait_seconds = 10
```
