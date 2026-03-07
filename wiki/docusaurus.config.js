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
            'LEGAL.md',
            'DISCLAIMER.md',
            'TRADEMARK.md',
            'SPONSORS.md',
            'hardware/LICENSE',
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
      title: '',
      logo: {
        alt: 'SimpleGo',
        src: 'img/logo.svg',
        href: '/',
        target: '_self',
      },
      items: [
        {to: '/', label: 'Docs', position: 'left', activeBaseRegex: '^/$'},
        {href: 'https://simplego.dev', label: 'Home', position: 'right'},
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
          title: 'Documentation',
          items: [
            {label: 'Getting Started', to: '/getting-started'},
            {label: 'Architecture', to: '/architecture'},
            {label: 'SMP in C', to: '/smp-in-c'},
            {label: 'Protocol Analysis', to: '/protocol-analysis'},
            {label: 'Hardware', to: '/hardware'},
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
            {label: 'Flash Tool', href: 'https://simplego.dev/flash.html'},
            {label: 'Network', href: 'https://simplego.dev/network.html'},
          ],
        },
        {
          title: 'Legal',
          items: [
            {label: 'Imprint', href: 'https://simplego.dev/imprint.html'},
            {label: 'Privacy Policy', href: 'https://simplego.dev/legal/privacy.html'},
            {label: 'Terms of Service', href: 'https://simplego.dev/legal/tos.html'},
            {label: 'Disclaimer', href: 'https://simplego.dev/legal/disclaimer.html'},
            {label: 'Law Enforcement', href: 'https://simplego.dev/legal/law-enforcement.html'},
            {label: 'Transparency Report', href: 'https://simplego.dev/legal/transparency.html'},
          ],
        },
        {
          title: 'Company',
          items: [
            {label: 'Partnership', href: 'https://simplego.dev/contact.html'},
            {label: 'Contact', href: 'mailto:contact@simplego.dev'},
          ],
        },
      ],
      copyright: `© ${new Date().getFullYear()} SimpleGo.dev -- S.D - IT and More Systems · Software: AGPL-3.0 · Hardware: CERN-OHL-W-2.0`,
    },
    prism: {
      theme: prismThemes.oneDark,
      darkTheme: prismThemes.oneDark,
      additionalLanguages: ['c', 'bash', 'powershell', 'haskell'],
    },
    docs: {
      sidebar: {
        hideable: true,
        autoCollapseCategories: true,
      },
    },
  }),
};

export default config;
