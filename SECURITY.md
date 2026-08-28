# Security Policy

## Supported Versions

We release patches for security vulnerabilities in the latest release only.  
Please ensure you are using the most recent version of **Nektum Shield** before reporting a vulnerability.

| Version | Supported          |
| ------- | ------------------ |
| 3.2.x   | :white_check_mark: |
| < 3.2   | :x:                |

## Reporting a Vulnerability

**Please do not open public issues for security vulnerabilities.**

Always report vulnerabilities, include as much detail as possible:

- Description of the vulnerability
- Steps to reproduce
- Affected version(s)
- Any possible mitigations

You will receive a response within **72 hours**.  
If the vulnerability is confirmed, we will work on a fix and release a patch as soon as possible.  
We kindly ask that you do not disclose the issue publicly until a fix has been released.

## Security Considerations for Server Administrators

- **Discord Webhook URL**: Set via the `ns_webhook_url` cvar. Treat this URL as a secret – anyone with it can post messages to your Discord channel. Do not share it publicly or commit it to a repository.
- **Banlist & User Databases**: The plugin stores data in plain text files (`nektumshield_banlist.txt`, `nektumshield_users.txt`, `nektumshield_mutes.txt`). Protect the server filesystem and ensure only trusted administrators have read/write access.
- **Admin Commands**: Commands `bb`, `ub`, `fu`, `mute`, `unmute` are protected by permission levels (80/60). Ensure your server’s admin system is configured correctly to prevent unauthorized use.
- **CGNAT Protection**: The plugin automatically skips subnet bans for CGNAT IP ranges to avoid banning innocent users. Be aware that this may allow ban evasion from mobile networks; consider additional verification methods.

## Known Limitations

- This is a **server‑side** anti‑cheat. It cannot detect all client‑side modifications, especially those that do not produce detectable behavioral anomalies.
- No anti‑cheat is perfect; use alongside other security layers and manual admin oversight.