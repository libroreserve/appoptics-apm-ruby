# Copyright (c) 2016 SolarWinds, LLC.
# All rights reserved.

if !defined?(JRUBY_VERSION)

  require 'minitest_helper'
  require 'webmock/minitest'
  require 'mocha/mini_test'
  require 'traceview/inst/rack'

  class FaradayMockedTest < Minitest::Test
    include Rack::Test::Methods

    def setup
      WebMock.enable!
      WebMock.disable_net_connect!
      TraceView.config_lock.synchronize {
        @sample_rate = TraceView::Config[:sample_rate]
      }
    end

    def teardown
      TraceView.config_lock.synchronize {
        TraceView::Config[:blacklist] = []
        TraceView::Config[:sample_rate] = @sample_rate
      }
      WebMock.reset!
      WebMock.allow_net_connect!
      WebMock.disable!
    end

    def test_tracing_sampling
      stub_request(:get, "http://127.0.0.1:8101/")

      TraceView::API.start_trace('faraday_test') do
        conn = Faraday.new(:url => 'http://127.0.0.1:8101') do |faraday|
          faraday.adapter  Faraday.default_adapter  # make requests with Net::HTTP
        end
        conn.get
      end

      assert_requested :get, "http://127.0.0.1:8101/", times: 1
      assert_requested :get, "http://127.0.0.1:8101/", headers: {'X-Trace'=>/^2B[0-9,A-F]*01$/}, times: 1
    end

    def test_tracing_not_sampling
      stub_request(:get, "http://127.0.0.2:8101/")

      TraceView.config_lock.synchronize do
        TraceView::Config[:sample_rate] = 0
        TraceView::API.start_trace('faraday_test') do
          conn = Faraday.new(:url => 'http://127.0.0.2:8101') do |faraday|
            faraday.adapter  Faraday.default_adapter  # make requests with Net::HTTP
          end
          conn.get
        end
      end

      assert_requested :get, "http://127.0.0.2:8101/", times: 1
      assert_requested :get, "http://127.0.0.2:8101/", headers: {'X-Trace'=>/^2B[0-9,A-F]*00$/}, times: 1
      assert_not_requested :get, "http://127.0.0.2:8101/", headers: {'X-Trace'=>/^2B0*$/}
    end

    def test_no_xtrace
      stub_request(:get, "http://127.0.0.3:8101/")

      conn = Faraday.new(:url => 'http://127.0.0.3:8101') do |faraday|
        faraday.adapter  Faraday.default_adapter  # make requests with Net::HTTP
      end
      conn.get

      assert_requested :get, "http://127.0.0.3:8101/", times: 1
      assert_not_requested :get, "http://127.0.0.3:8101/", headers: {'X-Trace'=>/^.*$/}
    end

    def test_blacklisted
      stub_request(:get, "http://127.0.0.4:8101/")

      TraceView.config_lock.synchronize do
        TraceView::Config.blacklist << '127.0.0.4'
        TraceView::API.start_trace('faraday_test') do
          conn = Faraday.new(:url => 'http://127.0.0.4:8101') do |faraday|
            faraday.adapter  Faraday.default_adapter  # make requests with Net::HTTP
          end
          conn.get
        end
      end

      assert_requested :get, "http://127.0.0.4:8101/", times: 1
      assert_not_requested :get, "http://127.0.0.4:8101/", headers: {'X-Trace'=>/^.*$/}
    end

  end
end
