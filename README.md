# check\_rest\_api  |   Nagios plugin to check a REST API                                                                                                                                                                                                                                                                                                                                                                                                                 
This program can be used with Nagios to hit a REST API and compare values to monitor services.

## Installation
Install this program by downloading the proper version for your operation system on the [releases](https://github.com/xWyatt/check_rest_api/releases) page, then drop the executable in the `libexec/` folder of your Nagios installation.

After installing this program you can create custom commands in Nagios to utilize it. Reference the Nagios user manual for more information on custom commands.
 
## Usage
The below usage assumes you are running the program stand-alone on a command line.
Usage for a Nagios Command will be nearly identical.
| &nbsp; &nbsp;Name/Option &nbsp; &nbsp;  | Shorthand | Description  |
|---|:-:|--|
| `--hostname` | `-H` | Full address to the REST API endpoint |
| `--auth-basic` | `-b` | A string in the form `<username>:<password>` that is used for HTTP Basic Auth |
| `--auth-basic-file` | `-bf` | Filepath to a file that contains one line in the format `<username>:<password>` that is used for HTTP Basic Auth
| `--key` | `-K` | A comma-delimited list of JSON keys to check. More detail on accessing JSON keys are below |
| `--critical` | `-c` | A comma-delimited list of 'critical' value criteria. Each entry corresponds to a `--key` entry. See Nagios Plugin documentation on critical values |
| `--warning` | `-w` | A comma-delimited list of 'warning' value criteria. Each entry corresponds to a `--key` entry. See Nagios Plugin documentation on warning values |
| `--timeout` | `-t` | Sets a custom timeout, in seconds, to abort the HTTP request should the remote server not respond. Defaults to `10` seconds. |
| `--insecure` | `-k` | Disables checking peer's SSL certificate (if using SSL/HTTPS). Not recommended to use |

### Example Usage
`Note`: Tildes (~) are escaped here as a normal BaSH will expand that to the users home directory. You needn't escape a tilde when writing a custom Nagios Command. 
```bash
# Check an API endpoint and send a 'critical' value if 
# the JSON key `cpu` is above `80`
./check_rest_api -H http://www.contoso.com/api/endpoint -K cpu -c \~:80

# Check an API endpoint and send a 'warning' value if the 
# JSON key `files` is less than `23`
./check_rest_api -H http://www.contoso.com/api/endpoint2 -K files 23:

# Check an API endpoint and send a 'warning' if the JSON key `ram` is above `75` 
# and a 'critical' if it is above `80`
./check_rest_api -H http://www.contoso.com/api/endpoint3 -K ram -w \~:75 -c \~:80
  
# Check an API endpoint. Send a 'warning' if `cpu` is above 
# 60 or `ram` is above `63`, and a 'critical' if `cpu` 
 #is above `70` or `ram` is above `83`
./check_rest_api -H http://www.contoso.com/api/endpoint4 -K cpu,ram -w \~:60,\~:63 -c \~:70,\~:83

# Check an API endpoint with HTTP Basic Auth (via CLI)
./check_rest_api -H http://www.contoso.com --auth-basic username:password

# Check an API endpoint with HTTP Basic Auth (with file). The file ./test has one line with the string "username:password" to use for HTTP Basic Auth
./check_rest_api -H http://www.contoso.com --auth-basic-file ./test
```  

## Compiling
To compile the program from scratch:
1. Clone/download this repository
2. Verify the below dependencies are installed in addition to a C89-compliant compiler and `make`
3. `cd` in the `src/` directory and run the command `make`
4. `make` will compile the program called `check_rest_api`

### Dependencies:
`check_rest_api` has only two external dependencies:
- [`libcurl`](https://curl.haxx.se/libcurl/) - for making HTTP/HTTPS calls and retrieving data from a REST API
- [`JSON-C`](https://github.com/json-c/json-c) - for parsing JSON results from an API                                                                                                                                                                                                                       - Make sure your dependencies are installed                                                                                                                                                                                                  - `make`                                                                                                                                                                                                                                                                                                                                                                                                                            
