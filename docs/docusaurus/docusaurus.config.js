// @ts-check

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'SimpleGo Docs',
  tagline: 'Native SimpleX Protocol on Bare-Metal Hardware',
  favicon: 'img/favicon.ico',

  url: 'https://docs.simplego.dev',
  baseUrl: '/',

  organizationName: 'saschadaemgen',
  projectName: 'SimpleGo',
  deploymentBranch: 'gh-pages',
  trailingSlash: false,

  onBrokenLinks: 'warn',
  onBrokenMarkdownLinks: 'warn',

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          sidebarPath: require.resolve('./sidebars.js'),
          routeBasePath: '/',
          editUrl: 'https://github.com/saschadaemgen/SimpleGo/edit/main/docs/docusaurus/',
        },
        blog: false,
        theme: {
          customCss: require.resolve('./src/css/custom.css'),
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
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
          href: 'https://simplego.dev',
          target: '_self',
        },
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'gettingStartedSidebar',
            position: 'left',
            label: 'Getting Started',
          },
          {
            type: 'docSidebar',
            sidebarId: 'architectureSidebar',
            position: 'left',
            label: 'Architecture',
          },
          {
            type: 'docSidebar',
            sidebarId: 'smpInCSidebar',
            position: 'left',
            label: 'SMP in C',
          },
          {
            type: 'docSidebar',
            sidebarId: 'hardwareSidebar',
            position: 'left',
            label: 'Hardware',
          },
          {
            type: 'docSidebar',
            sidebarId: 'securitySidebar',
            position: 'left',
            label: 'Security',
          },
          {
            type: 'docSidebar',
            sidebarId: 'referenceSidebar',
            position: 'left',
            label: 'Reference',
          },
          {
            href: 'https://github.com/saschadaemgen/SimpleGo',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'Docs',
            items: [
              { label: 'Getting Started', to: '/getting-started' },
              { label: 'SMP in C', to: '/smp-in-c' },
              { label: 'Architecture', to: '/architecture' },
            ],
          },
          {
            title: 'Project',
            items: [
              { label: 'simplego.dev', href: 'https://simplego.dev' },
              { label: 'GitHub', href: 'https://github.com/saschadaemgen/SimpleGo' },
              { label: 'X / Twitter', href: 'https://x.com/simplegodev' },
            ],
          },
          {
            title: 'Legal',
            items: [
              { label: 'Imprint', href: 'https://simplego.dev/imprint.html' },
              { label: 'Privacy Policy', href: 'https://simplego.dev/legal/privacy.html' },
            ],
          },
        ],
        copyright: `© ${new Date().getFullYear()} SimpleGo.dev — S.D - IT and More Systems · Software: AGPL-3.0 · Hardware: CERN-OHL-W-2.0`,
      },
      prism: {
        theme: require('prism-react-renderer').themes.dracula,
        additionalLanguages: ['c', 'bash', 'yaml', 'powershell', 'json'],
      },
      algolia: undefined,
    }),
};

module.exports = config;
