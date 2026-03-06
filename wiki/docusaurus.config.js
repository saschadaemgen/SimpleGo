// @ts-check
import {themes as prismThemes} from 'prism-react-renderer';

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'SimpleGo',
  tagline: 'Dedicated Secure Communication Devices',
  favicon: 'https://simplego.dev/favicon-32.png',
  url: 'https://wiki.simplego.dev',
  baseUrl: '/',
  organizationName: 'saschadaemgen',
  projectName: 'SimpleGo',
  onBrokenLinks: 'warn',
  onBrokenMarkdownLinks: 'warn',
  markdown: {
    format: 'detect',
  },

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      ({
        docs: {
          path: '../docs',
          routeBasePath: '/',
          sidebarPath: './sidebars.js',
          editUrl: 'https://github.com/saschadaemgen/SimpleGo/edit/main/docs/',
          exclude: [
            'protocol-analysis/**',
            'release-info/**',
            'hardware/**',
            'legal/**',
            'security/**',
            'BUGS.md',
            'BUILD_SYSTEM.md',
            'DEVELOPMENT.md',
            'DEVNOTES.md',
            'DISCLAIMER.md',
            'LEGAL.md',
            'SPONSORS.md',
            'TRADEMARK.md',
          ],
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  themeConfig: ({
    colorMode: {
      defaultMode: 'dark',
      disableSwitch: true,
      respectPrefersColorScheme: false,
    },
    navbar: {
      title: 'SimpleGo',
      logo: {
        alt: 'SimpleGo',
        src: 'https://simplego.dev/favicon-32.png',
        href: 'https://simplego.dev',
        target: '_self',
      },
      items: [
        {to: '/', label: 'Docs', position: 'left', activeBaseRegex: '^/$'},
        {to: '/getting-started', label: 'Getting Started', position: 'left'},
        {to: '/architecture', label: 'Architecture', position: 'left'},
        {to: '/smp-in-c', label: 'SMP in C', position: 'left'},
        {to: '/hardware', label: 'Hardware', position: 'left'},
        {to: '/security', label: 'Security', position: 'left'},
        {to: '/reference/constants', label: 'Reference', position: 'left'},
        {href: 'https://simplego.dev/product.html', label: 'Product', position: 'right'},
        {href: 'https://simplego.dev/crypto.html', label: 'Crypto', position: 'right'},
        {href: 'https://simplego.dev/flash.html', label: 'Flash Tool', position: 'right'},
        {href: 'https://simplego.dev/network.html', label: 'Network', position: 'right'},
        {href: 'https://github.com/saschadaemgen/SimpleGo', label: 'GitHub', position: 'right'},
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Docs',
          items: [
            {label: 'Getting Started', to: '/getting-started'},
            {label: 'SMP in C', to: '/smp-in-c'},
            {label: 'Architecture', to: '/architecture'},
            {label: 'Security', to: '/security'},
            {label: 'Reference', to: '/reference/constants'},
          ],
        },
        {
          title: 'Project',
          items: [
            {label: 'simplego.dev', href: 'https://simplego.dev'},
            {label: 'GitHub', href: 'https://github.com/saschadaemgen/SimpleGo'},
            {label: 'X / Twitter', href: 'https://x.com/simplegodev'},
          ],
        },
        {
          title: 'Legal',
          items: [
            {label: 'Imprint', href: 'https://simplego.dev/imprint.html'},
            {label: 'Privacy Policy', href: 'https://simplego.dev/legal/privacy.html'},
          ],
        },
      ],
      copyright: `© ${new Date().getFullYear()} SimpleGo.dev – S.D - IT and More Systems · Software: AGPL-3.0 · Hardware: CERN-OHL-W-2.0`,
    },
    prism: {
      theme: prismThemes.oneDark,
      darkTheme: prismThemes.oneDark,
      additionalLanguages: ['c', 'bash', 'powershell', 'haskell'],
    },
    docs: {
      sidebar: {
        hideable: true,
        autoCollapseCategories: false,
      },
    },
  }),
};

export default config;
