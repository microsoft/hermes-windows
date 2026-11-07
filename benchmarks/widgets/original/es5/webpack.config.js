/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

module.exports = {
  mode: "production",
  target: ['web', 'es5'],
  output: {
    path: __dirname,
    filename: 'widgets.js',
  },
  optimization: {
    minimize: false,
  },
  devtool: 'source-map',
  module: {
    rules: [
      {
        test: /\.js$/,
        exclude: /node_modules/,
        use: {
          loader: 'babel-loader',
          options: {
            plugins: [
              ['@babel/plugin-proposal-class-properties', {"loose": true}],
              '@babel/plugin-transform-flow-strip-types',
              ["@babel/plugin-transform-classes", {"loose": true}]
            ]
          }
        }
      }
    ]
  }
};
