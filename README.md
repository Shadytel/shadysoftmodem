# shadysoftmodem

An external YATE module that wraps the slmodem code and binaries. Still highly experimental.

Here's some sample lines to drop in yate's regexroute.conf:

```
^4786$=external/playrec//home/supersat/shadysoftmodem/inbound_modem_attach /home/supersat/shadysoftmodem/scripts/incoming-pppd-ipv6-1200
^4227$=external/playrec//home/supersat/shadysoftmodem/inbound_modem_attach /home/supersat/shadysoftmodem/scripts/incoming-undercurrents-1200
^2329$=external/playrec//home/supersat/shadysoftmodem/inbound_modem /home/supersat/shadysoftmodem/scripts/incoming-fax
```

The behavior of the inbound_modem program is slightly dependent on the name it is called with:

- `inbound_modem` will simply establish a /dev/pts device and pass it as an argument to the program specified
- `inbound_modem_attach` will attach the new /dev/pts device to stdin/stdout, then execute the program specified. This makes it very easy to interact with programs like telnet, pppd, etc.
