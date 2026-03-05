// @ts-check

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  docs: [
    {
      type: 'category',
      label: 'Architecture',
      items: ['ARCHITECTURE'],
    },
    {
      type: 'category',
      label: 'SMP in C',
      items: ['CRYPTO', 'PROTOCOL', 'WIRE_FORMAT'],
    },
    {
      type: 'category',
      label: 'Hardware',
      items: ['ADDING_NEW_DEVICE'],
    },
    {
      type: 'category',
      label: 'Security',
      items: ['SECURITY_MODEL'],
    },
    {
      type: 'category',
      label: 'Why SimpleGo',
      items: ['SIMPLEGO_VS_GRAPHENEOS', 'SIMPLEX_VS_MATRIX'],
    },
    {
      type: 'category',
      label: 'Reference',
      items: ['TECHNICAL'],
    },
  ],
};

export default sidebars;
