# Security requirements

- Use HTTPS for all internet-facing traffic. Keep API and Jellyfin bound to localhost behind a correctly configured reverse proxy.
- Provision a unique device credential; do not embed admin keys in the image. Add ownership, expiry/rotation and revocation.
- Jellyfin credentials must be scoped to an authorized user, never a global administrative key sent to clients.
- Keep `.env`, database files, bearer tokens and media outside source control; redact logs.
- Add migrations, authentication rate limiting, request validation, authorization tests and backups before deployment.
- Cloud firewall and container isolation must be explicitly configured; default Docker Compose here is for local testing.
- Distribute only media and games you have rights to distribute.
