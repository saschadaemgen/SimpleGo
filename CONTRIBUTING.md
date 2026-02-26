# Contributing to SimpleGo

Thank you for considering contributing to SimpleGo!

---

## Code of Conduct

This project is governed by our Code of Conduct (CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

---

## Getting Started

### Before You Start

1. Check existing issues - Your idea might already be discussed
2. Read the documentation - Familiarize yourself with the README
3. Discuss first - For significant changes, open an issue before coding

### Types of Contributions

| Contribution | Description |
|--------------|-------------|
| Bug Reports | Found a bug? Let us know! |
| Feature Requests | Have an idea? We would love to hear it! |
| Documentation | Typos, clarifications, examples |
| Code | Bug fixes, features, refactoring |
| Hardware Testing | Test on different ESP32 boards |

---

## Development Setup

### Prerequisites

| Component | Version |
|-----------|---------|
| ESP-IDF | 5.5.2+ |
| Python | 3.8+ |
| Hardware | ESP32-S3 (T-Deck recommended) |

### Build and Flash

Run: idf.py build flash monitor -p COM5

---

## Commit Message Format

We use Conventional Commits for clear, consistent history.

### Format

type(scope): description

### Types

| Type | Description |
|------|-------------|
| feat | New feature |
| fix | Bug fix |
| docs | Documentation |
| refactor | Code restructuring |
| test | Adding tests |
| chore | Maintenance |

### Scopes

| Scope | Description |
|-------|-------------|
| crypto | Cryptography (X448, ratchet, AES) |
| network | TLS, TCP, SMP protocol |
| peer | Peer connection |
| parser | Message parsing |
| queue | Queue management |

### Examples

- feat(crypto): add X448 key generation
- fix(ratchet): correct IV order in chain KDF
- docs(readme): update installation instructions

---

## Pull Request Process

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Sign your commits with DCO (git commit -s)
5. Push to your fork
6. Open a Pull Request

---

## Developer Certificate of Origin (DCO)

We use the DCO to ensure contributions can be legally distributed.

### How to Sign Off

Add -s flag to your commits: git commit -s -m "feat(crypto): add new feature"

This adds a Signed-off-by line to your commit.

---

## Recognition

Contributors are recognized in:
- Release notes
- README.md contributors section

---

## License

By contributing, you agree that your contributions will be licensed under the AGPL-3.0 License.

---

Thank you for contributing to SimpleGo!

Privacy is not a privilege, it is a right.
