/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  gettingStartedSidebar: [
    {
      type: 'category',
      label: 'Getting Started',
      collapsed: false,
      items: [
        'getting-started/index',
        'getting-started/quick-start',
        'getting-started/flashing',
        'getting-started/building',
        'getting-started/faq',
      ],
    },
  ],

  architectureSidebar: [
    {
      type: 'category',
      label: 'Architecture',
      collapsed: false,
      items: [
        'architecture/index',
        'architecture/system-overview',
        'architecture/encryption-layers',
        'architecture/memory-layout',
        'architecture/crypto-state',
        'architecture/wifi-manager',
      ],
    },
  ],

  smpInCSidebar: [
    {
      type: 'category',
      label: 'SMP in C',
      collapsed: false,
      items: [
        'smp-in-c/index',
        'smp-in-c/overview',
        'smp-in-c/transport',
        'smp-in-c/queue-lifecycle',
        'smp-in-c/handshake',
        'smp-in-c/encryption',
        'smp-in-c/ratchet',
        'smp-in-c/idempotency',
        'smp-in-c/subscription',
        'smp-in-c/pitfalls',
      ],
    },
  ],

  hardwareSidebar: [
    {
      type: 'category',
      label: 'Hardware',
      collapsed: false,
      items: [
        'hardware/index',
        'hardware/t-deck-plus',
        'hardware/hardware-tiers',
        'hardware/component-selection',
        'hardware/hal-architecture',
        'hardware/pcb-design',
        'hardware/enclosure-design',
        'hardware/hardware-security',
        'hardware/adding-new-device',
      ],
    },
  ],

  securitySidebar: [
    {
      type: 'category',
      label: 'Security',
      collapsed: false,
      items: [
        'security/index',
        'security/security-model',
        'security/threat-model',
        'security/hardware-security',
        'security/audit-log',
      ],
    },
  ],

  whySimpleGoSidebar: [
    {
      type: 'category',
      label: 'Why SimpleGo',
      collapsed: false,
      items: [
        'why-simplego/index',
        'why-simplego/vs-grapheneos',
        'why-simplego/vs-matrix',
        'why-simplego/vs-signal',
        'why-simplego/vs-briar',
      ],
    },
  ],

  contributingSidebar: [
    {
      type: 'category',
      label: 'Contributing',
      collapsed: false,
      items: [
        'contributing/index',
        'contributing/build',
        'contributing/project-structure',
        'contributing/coding-standards',
        'contributing/license',
      ],
    },
  ],

  referenceSidebar: [
    {
      type: 'category',
      label: 'Reference',
      collapsed: false,
      items: [
        'reference/constants',
        'reference/crypto-primitives',
        'reference/wire-format',
        'reference/nvs-key-registry',
        'reference/protocol-links',
        'reference/changelog',
        'reference/roadmap',
      ],
    },
  ],
};

module.exports = sidebars;
